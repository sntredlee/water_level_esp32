#include "sd_card.h"

static bool _sd_card_ok = false;
static fs::FS &sd_fs = SD;

// **Function to Read and Parse Configuration File**
config_t readConfigFile() {  
  const char *path = "/config.txt";  

  config_t sys_config;

  if (!_sd_card_ok){
    Serial.printf("SD card is not installed/initialized, returning default config");
    Serial.flush();  
    return sys_config;
  }else{
    Serial.printf("Reading config file: %s\n", path);
    Serial.flush();
  }

  File file = sd_fs.open(path);
  if (!file) {
    Serial.println("Failed to open config file for reading!");
    Serial.flush();
    return sys_config;
  }

  while (file.available()) {
    String line = file.readStringUntil('\n');  // Read a line from file
    line.trim();                               // Remove whitespace and newline characters

    if (line.startsWith("loop_period_sec:")) {
      sys_config.loop_period_sec = line.substring(line.indexOf(":") + 1).toInt();
    } else if (line.startsWith("max_sec_between_logs:")) {
      sys_config.max_sec_between_logs = line.substring(line.indexOf(":") + 1).toInt();
    } else if (line.startsWith("max_sec_between_comm:")) {
      sys_config.max_sec_between_comm = line.substring(line.indexOf(":") + 1).toInt();
    } else if (line.startsWith("wifi_ssid:")) {
      sys_config.ssid = line.substring(line.indexOf(":") + 1);
    } else if (line.startsWith("wifi_password:")) {
      sys_config.password = line.substring(line.indexOf(":") + 1);
    } else if (line.startsWith("telegram_token:")) {
      sys_config.telegram_botToken = line.substring(line.indexOf(":") + 1);
    } else if (line.startsWith("telegram_chatID:")) {
      sys_config.telegram_chatID = line.substring(line.indexOf(":") + 1);
    }
  }

  file.close();

  // Print parsed values
  Serial.println("Configuration Loaded:");
  Serial.printf("  loop_period_sec: %d\n", sys_config.loop_period_sec);
  Serial.printf("  max_sec_between_logs: %d\n", sys_config.max_sec_between_logs);
  Serial.printf("  max_sec_between_comm: %d\n", sys_config.max_sec_between_comm);
  Serial.printf("  WiFi ssid: %s\n", sys_config.ssid.c_str());
  Serial.printf("  WiFi password: %s\n", sys_config.password.c_str());
  Serial.printf("  Telegram bot token: %s\n", sys_config.telegram_botToken.c_str());
  Serial.printf("  Telegram chat ID: %s\n", sys_config.telegram_chatID.c_str());
  Serial.flush();

  return sys_config;
}


bool init_sd_card(){ 
  // Initialize SD card  
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);

  if (!SD.begin(SD_CS)) {
    Serial.println("Card Mount Failed!");    
    _sd_card_ok = false;
  } else {
    Serial.println("SD Card Initialized.");        
    _sd_card_ok = true;
  }

  Serial.flush();
  return is_sd_card_ok;
}


bool is_sd_card_ok(){
  return _sd_card_ok;
}

// **Function to Write to a File**
bool writeFile(const char *path, const char *message) {
  bool ret = false;
#if DEBUG
  Serial.printf("Writing file: %s\n", path);
  Serial.flush();
#endif

  File file = sd_fs.open(path, FILE_WRITE);
  if (!file) {
#if DEBUG
    Serial.println("Failed to open file for writing!");
    Serial.flush();
#endif
    return false;
  }
  if (file.print(message)) {
#if DEBUG
    Serial.println("File written successfully.");
    Serial.flush();
#endif
    ret = true;
  } else {
#if DEBUG
    Serial.println("Write failed.");
    Serial.flush();
#endif
    ret = false;
  }
  file.close();
  return ret;
}


// **Function to Append Data to a File**
bool appendFile(const char *path, const char *message) {
  // return true: success , false: fail
  bool ret = false;

#if DEBUG
  Serial.printf("Appending to file: %s\n", path);
  Serial.flush();
#endif

  File file = sd_fs.open(path, FILE_APPEND);
  if (!file) {
#if DEBUG
    Serial.println("Failed to open file for appending!");
    Serial.flush();
#endif
    return false;
  }

  if (file.print(message)) {
#if DEBUG
    Serial.println("Append successful.");
    Serial.flush();
#endif
    ret = true;
  } else {
#if DEBUG
    Serial.println("Append failed.");
    Serial.flush();
#endif
    ret = false;
  }
  file.close();
  return ret;
}