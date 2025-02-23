#include <WiFi.h>
// #include <WiFiUdp.h>
#include <FS.h>
#include <SD.h>
#include <SPI.h>
#include "esp_wifi.h"
#include "time.h"
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>

/*
Note about telegram:

ArduinoJson version 6.16.0  (Version 7 will not work!!!)
UniversalTelegramBot : version 1.3.0


Two sleep modes:
  Light Sleep:
      CPU: paused
      RAM: retained
      WiFi: can be ON
      Wake up time: 1-2ms   
      Power consumption: 5-10mA
      millis() continues counting after wake-up
    esp_sleep_enable_timer_wakeup(1000000); // Sleep for 1 second
    esp_light_sleep_start();

  Deep Sleep:
      CPU: off
      RAM: off , use RTC_DATA_ATTR to retain values of variables
      WiFi: off
      Wake up time: 150ms (reboots)
      Power consumption: 0.1-1mA 
      millis() resets to 0 after wake-up (ESP32 reboots)
    esp_sleep_enable_timer_wakeup(1000000); // Wake up in 1 second
    esp_deep_sleep_start();

  Keep track of time:
    esp_timer_get_time()  Returns time in microseconds since boot.
                          Continues increasing during light sleep but resets after deep sleep.
    Note: millis() stops increasing in light sleep mode

   Use TCP instead of UDP to report water level, as UDP over ESP32 is not reliable. -- No longer report water level to Raspberry Pi, running remote command on chip.
*/

#define DEBUG 1

uint8_t loop_period_sec = 1;
uint8_t max_sec_between_logs = 10;
uint8_t max_sec_between_comm = 15;
uint32_t max_sec_between_ntp = 7 * 24 * 3600;
String ssid = "OPTUSVD3644ED0";
String password = "ALGAEBOMBS08954";

// **Telegram Bot Credentials**
String telegram_botToken = "7646712534:AAH-FfQ82DJTWUbYglw2rOtWhhjGo2zclFM";
String telegram_chatID = "7108046311";
WiFiClientSecure secure_client;
UniversalTelegramBot *bot = nullptr;

// LED flash: WiFi activitiy or SD card logging activitiy
#define LED_PIN 5  
#define SD_CS 4


// RTC Memory allows keeping variables after deep sleep
RTC_DATA_ATTR uint32_t loops_since_boot = 0;
RTC_DATA_ATTR int8_t last_water_level = -1;
RTC_DATA_ATTR int8_t curr_water_level = -1;
RTC_DATA_ATTR uint32_t loops_since_last_2to1 = 0;
RTC_DATA_ATTR uint32_t last_pump_op_time_in_loops = 0;
RTC_DATA_ATTR uint64_t loops_since_last_log = 0;
RTC_DATA_ATTR uint64_t loops_since_last_comm = 0;
RTC_DATA_ATTR uint64_t loops_since_last_ntp = 0;

uint64_t loop_period_us;
uint8_t wifi_on = 0; // 1: wifi turned on, 0: wifi is off
uint8_t sd_card_ok = 0;

// **NTP Server for RTC Sync**
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 36000 + 3600;  // Adjust for your timezone (e.g., -18000 for EST)
const long  daylightOffset_sec = 3600;
// **Initialize RTC Time**
struct tm timeinfo;
// **Format the timestamp**
char timeString[30];


void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);  // Set LED pin as output
  digitalWrite(LED_PIN, HIGH); // HIGH-> OFF  LOW-> ON  

  pinMode(32, OUTPUT);
  pinMode(33, INPUT_PULLDOWN);
  pinMode(25, INPUT_PULLDOWN);
  pinMode(26, INPUT_PULLDOWN);
  pinMode(27, INPUT_PULLDOWN);
  pinMode(14, INPUT_PULLDOWN);  
  
  // Initialize SD card
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);
  if (!SD.begin(SD_CS)){
    sd_card_ok = 0;
    Serial.println("Card Mount Failed!");
    Serial.flush();
  }else{
    sd_card_ok = 1;
    Serial.println("SD Card Initialized.");
    Serial.flush();
    readConfigFile(SD, "/config.txt");
  }

  loop_period_us = loop_period_sec * 1000000;
  
  // **Initialize RTC from NTP**
  sync_rtc();

  // secure_client.setCACert(TELEGRAM_CERTIFICATE_ROOT); // Add root certificate for api.telegram.org
  secure_client.setInsecure();
  bot = new UniversalTelegramBot(telegram_botToken.c_str(), secure_client);
  sendTelegramMessage("Hello! I am online.");
}


void loop() {  
  uint64_t ts_loop_start = esp_timer_get_time();
  
  // Task 1: read latest water level
  curr_water_level = read_water_level();
#if DEBUG  
  Serial.print("\n\n\n Last Water Level: "); Serial.print(last_water_level);
  Serial.print("\t New Water Level: ");  Serial.println(curr_water_level);      
  Serial.flush();
#endif
  check_pump_operation();

  // Task 2: log the water level, if needed
  if ((curr_water_level != last_water_level) || (loops_since_last_log > max_sec_between_logs / loop_period_sec)) {
    loops_since_last_log = 0;
    log_water_level(curr_water_level);
  }else{
    loops_since_last_log++;
  }
  
  // Task 3: get online for remote command handling
  if (loops_since_last_comm > max_sec_between_comm / loop_period_sec) {
    loops_since_last_comm = 0;
    handle_comm();
  }else{
    loops_since_last_comm++;
  }
  
  // Task 4: NTP
  if (loops_since_last_ntp > max_sec_between_ntp / loop_period_sec){
    loops_since_last_ntp = 0;
    sync_rtc();
  }else{
    loops_since_last_ntp++;
  }

  // Finally, update last_water_level variable
  last_water_level = curr_water_level;
  loops_since_boot++;

  // Prepare to go to sleep  
  uint64_t loop_cost_us = esp_timer_get_time() - ts_loop_start;

#if DEBUG  
  Serial.print("Loop time (us): ");
  Serial.println(loop_cost_us);
  Serial.flush();
#endif

  if (loop_cost_us >= loop_period_us){
    // This loop takes too long, no sleep, go to next loop immediately
    // delay(1 ms) is called for the backend to do something.
    delay(1);
   }else{
    // light sleep is more power efficient because we need to wake up frequently and the power cost of rebooting (150ms @ 200mA) is much more
    turn_off_wifi();
  #if DEBUG  
    Serial.print("go to light sleep mode...");
    Serial.flush();
  #endif
    esp_sleep_enable_timer_wakeup(loop_period_us - loop_cost_us);
    esp_light_sleep_start();
   }
}

void check_pump_operation(){
  if (curr_water_level >= last_water_level){    
    loops_since_last_2to1 += 1;    
    return;
  }
  if ((curr_water_level==1) && (last_water_level==2)){
    // Level 2 -> 1
    loops_since_last_2to1 = 0;
  }else if ((curr_water_level==0) && (last_water_level==1)){    
    // Level 1 -> 0
    last_pump_op_time_in_loops = loops_since_last_2to1;
  }
}

void sync_rtc(){
  if (!wifi_on){
    turn_on_wifi();
  }
  if (!wifi_on){    
    return;
  }
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
#if DEBUG  
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");      
    Serial.flush();    
  }else{
    Serial.println("NTP time synchronized.");      
    Serial.flush();    
  }
#endif
}

int8_t read_water_level() {
  /*
  IO32 (JP1 pin 6)      COM
  IO33 (JP1 pin 7)      Lv1
  IO25 (JP1 pin 8)      Lv2
  IO26 (JP1 pin 9)      Lv3
  IO27 (JP1 pin 10)     Lv4
  IO14 (JP1 pin 11)     Lv5
  IO12 (JP1 pin 12)     Nil
  IO13 (JP1 pin 13)     Nil
  */
  int8_t lvl = 0;
  digitalWrite(32, HIGH);
  if (digitalRead(14)){
    lvl = 5;
  }else if (digitalRead(27)){
    lvl = 4;
  }else if (digitalRead(26)){
    lvl = 3;
  }else if (digitalRead(25)){
    lvl = 2;
  }else if (digitalRead(33)){
    lvl = 1;
  }  
  digitalWrite(32, LOW);

  return lvl;
}


void log_water_level(int8_t latest_water_lvl){
  digitalWrite(LED_PIN, LOW); 
  
  char buffer[64];  // Buffer for formatted string   
  getLocalTime(&timeinfo); 
  strftime(timeString, sizeof(timeString), "%Y-%m-%d %H:%M:%S", &timeinfo);
  snprintf(buffer, sizeof(buffer), "%s--%d\n", timeString, curr_water_level);    
#if DEBUG
  Serial.printf("logging info: %s\n", buffer);
#endif

  if (sd_card_ok){    
#if DEBUG
    Serial.printf("SD card OK, logging.\n");
    Serial.flush();
#endif
    if (!appendFile(SD, "/water_level_log.txt", buffer)){
#if DEBUG
      Serial.printf("Appending failed, try writing..\n");
      Serial.flush();
#endif
      writeFile(SD, "/water_level_log.txt", buffer);
    }
  }else{
#if DEBUG
    Serial.printf("SD card not OK, skip logging.\n");
    Serial.flush();
#endif
  }
  
  digitalWrite(LED_PIN, HIGH); 
}

void handle_comm(){
  digitalWrite(LED_PIN, LOW); 
  
  if (!wifi_on){
    turn_on_wifi();
  }
  if (!wifi_on){    
    return;
  }

  int messageCount = bot->getUpdates(bot->last_message_received + 1);
#if DEBUG
  Serial.printf("message count: %d\n", messageCount);
#endif
  
  while (messageCount) {
#if DEBUG
      Serial.println("New Telegram Message Received!");
      Serial.flush();
#endif
      for (int i = 0; i < messageCount; i++) {
          String text = bot->messages[i].text;
          String sender = bot->messages[i].from_name;
#if DEBUG
          Serial.print("Message from ");
          Serial.print(sender);
          Serial.print(": ");
          Serial.println(text);
          Serial.flush();
#endif
          if (text == "state") {
              getLocalTime(&timeinfo); 
              strftime(timeString, sizeof(timeString), "%Y-%m-%d %H:%M:%S", &timeinfo);              
              sendTelegramMessage("Water level at %s : %d", timeString, curr_water_level);
              if (0 != last_pump_op_time_in_loops){
                sendTelegramMessage("Last pump op %d H %d M %d S ago, took %d sec", 
                  loops_since_last_2to1 * loop_period_sec/ 3600, (loops_since_last_2to1 * loop_period_sec/ 60) % 60, (loops_since_last_2to1 * loop_period_sec) % 60,
                  last_pump_op_time_in_loops * loop_period_sec
                );
              }
          }
          else if (text == "uptime"){
            sendTelegramMessage("Up %d days %02d:%02d:%2d since boot", 
                  (loops_since_boot * loop_period_sec / 3600 / 24),
                  (loops_since_boot * loop_period_sec/ 3600) % 24, (loops_since_boot * loop_period_sec/ 60) % 60, (loops_since_boot * loop_period_sec) % 60                  
                );              
          }
          else if (text == "on") {
              // digitalWrite(LED_PIN, HIGH);
              // sendTelegramMessage("LED is now ON.");
          } 
          else if (text == "off") {
              // digitalWrite(LED_PIN, LOW);
              // sendTelegramMessage("LED is now OFF.");
          } 
          else {
              sendTelegramMessage("Unknown command..");
          }
      }

      messageCount = bot->getUpdates(bot->last_message_received + 1);
  }

  digitalWrite(LED_PIN, HIGH); 
}


uint8_t turn_on_wifi(){
  // return 1: wifi turned on successfully
  //        0: wifi turn on failed
#if DEBUG
  Serial.println("Connecting to WiFi...");
  Serial.flush();
#endif
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str());

  unsigned long startAttemptTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 500) { // 0.5 timeout
      delay(50);
  #if DEBUG
      Serial.print(".");
      Serial.flush();
  #endif
  }

  if (WiFi.status() == WL_CONNECTED) {
    wifi_on = 1;
#if DEBUG
    Serial.println("\nWiFi Connected!");
    Serial.flush();    
#endif
    return 1;  
  } else {
    wifi_on = 0;
#if DEBUG
    Serial.println("WiFi Connection Failed.");
    Serial.flush();    
#endif
    return 0;
  }
}

void turn_off_wifi(){
  if (wifi_on){
    // Disable WiFi to save power
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);    
    wifi_on = 0;
#if DEBUG
    Serial.println("WiFi turned Off.");
    Serial.flush();
#endif
  }
}


// **Function to Write to a File**
void writeFile(fs::FS &fs, const char *path, const char *message) {
#if DEBUG
    Serial.printf("Writing file: %s\n", path);
    Serial.flush();
#endif
    File file = fs.open(path, FILE_WRITE);
    if (!file) {
#if DEBUG
        Serial.println("Failed to open file for writing!");
        Serial.flush();
#endif
        return;
    }
    if (file.print(message)) {
#if DEBUG
        Serial.println("File written successfully.");
        Serial.flush();
#endif
    } else {
#if DEBUG
        Serial.println("Write failed.");
        Serial.flush();
#endif
    }
    file.close();
}


// **Function to Read and Parse Configuration File**
void readConfigFile(fs::FS &fs, const char *path) {
    Serial.printf("Reading config file: %s\n", path);
    Serial.flush();
    
    File file = fs.open(path);
    if (!file) {
        Serial.println("Failed to open config file for reading!");
        Serial.flush();
        return;
    }

    while (file.available()) {
        String line = file.readStringUntil('\n'); // Read a line from file
        line.trim();  // Remove whitespace and newline characters

        if (line.startsWith("loop_period_sec:")) {
            loop_period_sec = line.substring(line.indexOf(":") + 1).toInt();
        } 
        else if (line.startsWith("max_sec_between_logs:")) {
            max_sec_between_logs = line.substring(line.indexOf(":") + 1).toInt();
        } 
        else if (line.startsWith("max_sec_between_comm:")) {
            max_sec_between_comm = line.substring(line.indexOf(":") + 1).toInt();
        } 
        else if (line.startsWith("wifi_ssid:")) {
            ssid = line.substring(line.indexOf(":") + 1);
        } 
        else if (line.startsWith("wifi_password:")) {
            password = line.substring(line.indexOf(":") + 1);
        }
        else if (line.startsWith("telegram_token:")) {
            telegram_botToken = line.substring(line.indexOf(":") + 1);
        }
        else if (line.startsWith("telegram_chatID:")) {
            telegram_chatID = line.substring(line.indexOf(":") + 1);
        } 
    }
    
    file.close();
    
    // Print parsed values
    Serial.println("Configuration Loaded:");
    Serial.printf("  loop_period_sec: %d\n", loop_period_sec);
    Serial.printf("  max_sec_between_logs: %d\n", max_sec_between_logs);
    Serial.printf("  max_sec_between_comm: %d\n", max_sec_between_comm);
    Serial.printf("  WiFi ssid: %s\n", ssid.c_str());
    Serial.printf("  WiFi password: %s\n", password.c_str());
    Serial.printf("  Telegram bot token: %s\n", telegram_botToken.c_str());
    Serial.printf("  Telegram chat ID: %s\n", telegram_chatID.c_str());
    Serial.flush();
}

// **Function to Append Data to a File**
uint8_t appendFile(fs::FS &fs, const char *path, const char *message) {
  // return 1: success , 0: fail
    uint8_t ret = 0;
#if DEBUG
    Serial.printf("Appending to file: %s\n", path);
    Serial.flush();
#endif
    File file = fs.open(path, FILE_APPEND);
    if (!file) {
#if DEBUG
        Serial.println("Failed to open file for appending!");
        Serial.flush();
#endif        
        return 0;
    }
    if (file.print(message)) {
#if DEBUG      
        Serial.println("Append successful.");
        Serial.flush();
#endif        
        ret = 1;
    } else {
#if DEBUG      
        Serial.println("Append failed.");
        Serial.flush();
#endif        
        ret = 0;
    }
    file.close();
    return ret;
}

bool sendTelegramMessage(const char* format, ...) {
    
    if (!wifi_on){
      turn_on_wifi();
    }
    if (!wifi_on){    
      return false;
    }

    // **Format the message using snprintf()**
    char message[256];  // Buffer for formatted message
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);

#if DEBUG
    Serial.printf("Send %s to Telegram.", message);
    Serial.flush();
#endif

    return bot->sendMessage(telegram_chatID, message, "");
}