#include "time.h"
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
// #include "esp_system.h"  // Required for reset reason

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



uint8_t loop_period_sec = 1;
uint8_t max_sec_between_logs = 10;
uint8_t max_sec_between_comm = 15;
uint32_t max_sec_between_ntp = 7 * 24 * 3600;
String ssid = "OPTUSVD3644ED0";
String password = "ALGAEBOMBS08954";

// **Telegram Bot Credentials**
String telegram_botToken = "7646712534:AAH-FfQ82DJTWUbYglw2rOtWhhjGo2zclFM";
String telegram_chatID = "7108046311";



// LED flash: WiFi activitiy or SD card logging activitiy
#define LED_PIN 5
#define WARN_PIN 15


// RTC Memory allows keeping variables after deep sleep
RTC_DATA_ATTR uint32_t loops_since_boot = 0;
RTC_DATA_ATTR uint64_t loops_since_last_comm = 0;
RTC_DATA_ATTR uint64_t loops_since_last_ntp = 0;
RTC_DATA_ATTR bool warning_mesg_sent = false;

uint64_t loop_period_us;
uint8_t wifi_on = 0;  // 1: wifi turned on, 0: wifi is off
uint8_t sd_card_ok = 0;

// **NTP Server for RTC Sync**
const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 36000 + 3600;  // Adjust for your timezone (e.g., -18000 for EST)
const long daylightOffset_sec = 3600;
// **Initialize RTC Time**
struct tm timeinfo;
// **Format the timestamp**
char timeString[30];


// const char *getResetReasonString(esp_reset_reason_t reason) {
//   switch (reason) {
//     case ESP_RST_UNKNOWN: return "Unknown Reset";
//     case ESP_RST_POWERON: return "Power-On Reset";
//     case ESP_RST_EXT: return "External Pin Reset";
//     case ESP_RST_SW: return "Software Reset";
//     case ESP_RST_PANIC: return "Panic Reset (Exception)";
//     case ESP_RST_INT_WDT: return "Interrupt Watchdog Reset";
//     case ESP_RST_TASK_WDT: return "Task Watchdog Reset";
//     case ESP_RST_WDT: return "Watchdog Reset";
//     case ESP_RST_DEEPSLEEP: return "Wakeup from Deep Sleep";
//     case ESP_RST_BROWNOUT: return "Brownout Reset (Low Power)";
//     case ESP_RST_SDIO: return "SDIO Reset";
//     default: return "Unknown Reset Reason";
//   }
// }



void setup() {
  Serial.begin(115200);

  // esp_reset_reason_t reason = esp_reset_reason();
  // Serial.print("Last reset reason: ");
  // Serial.println(reason);

  pinMode(LED_PIN, OUTPUT);     // Set LED pin as output
  digitalWrite(LED_PIN, HIGH);  // HIGH-> OFF  LOW-> ON


  pinMode(WARN_PIN, OUTPUT);  // warning when set high drive buzzer etc.
  digitalWrite(WARN_PIN, LOW);


  loop_period_us = loop_period_sec * 1000000;

  // **Initialize RTC from NTP**
  sync_rtc();

}


void loop() {
  uint64_t ts_loop_start = esp_timer_get_time();

  // Task 1: read latest water level
  curr_water_level = read_water_level();
#if DEBUG
  Serial.print("\n\n\n Last Water Level: ");
  Serial.print(last_water_level);
  Serial.print("\t New Water Level: ");
  Serial.println(curr_water_level);
  Serial.flush();
#endif
  check_pump_operation();

  // Task 2: log the water level, if needed


  // Task 3: get online for remote command handling
  if (loops_since_last_comm > max_sec_between_comm / loop_period_sec) {
    loops_since_last_comm = 0;
    handle_comm();
  } else {
    loops_since_last_comm++;
  }

  // Task 4: NTP
  if (loops_since_last_ntp > max_sec_between_ntp / loop_period_sec) {
    loops_since_last_ntp = 0;
    sync_rtc();
  } else {
    loops_since_last_ntp++;
  }

  // Task 5: Alarm ?
  if (curr_water_level >= 4) {
    if (warning_mesg_sent) {
      // warning message already sent, will not repeatly send
    } else {
      warning_mesg_sent = sendTelegramMessage("WARNING : water level reached %d !!!", curr_water_level);
    }
    digitalWrite(WARN_PIN, HIGH);
  } else {
    // clear the warning flag, so new warning message will be sent when the level rise to critical again
    warning_mesg_sent = false;
    digitalWrite(WARN_PIN, LOW);
  }

  // Finally, update last_water_level variable
  loops_since_boot++;

  // Prepare to go to sleep
  uint64_t loop_cost_us = esp_timer_get_time() - ts_loop_start;

#if DEBUG
  Serial.print("Loop time (us): ");
  Serial.println(loop_cost_us);
  Serial.flush();
#endif

  if (loop_cost_us >= loop_period_us) {
    // This loop takes too long, no sleep, go to next loop immediately
    // delay(1 ms) is called for the backend to do something.
    delay(1);
  } else {
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


// Define command structure
typedef struct {
    const char* command;
    void (*function)();
    const char* description;
} TelegramCommand;

// Command function implementations
void cmd_state() {
    getLocalTime(&timeinfo);
    strftime(timeString, sizeof(timeString), "%Y-%m-%d %H:%M:%S", &timeinfo);
    sendTelegramMessage("Water level at %s : %d", timeString, curr_water_level);
    
    if (last_pump_op_time_in_loops != 0) {
        sendTelegramMessage("Last pump op %d days %2d:%2d:%2d ago, took %d sec",
                            (loops_since_boot * loop_period_sec / 3600 / 24),
                            loops_since_last_2to1 * loop_period_sec / 3600,
                            (loops_since_last_2to1 * loop_period_sec / 60) % 60,
                            (loops_since_last_2to1 * loop_period_sec) % 60,
                            last_pump_op_time_in_loops * loop_period_sec);
    }
}

void cmd_uptime() {
    sendTelegramMessage("Up %d days %02d:%02d:%02d since boot",
                        (loops_since_boot * loop_period_sec / 3600 / 24),
                        (loops_since_boot * loop_period_sec / 3600) % 24,
                        (loops_since_boot * loop_period_sec / 60) % 60,
                        (loops_since_boot * loop_period_sec) % 60);
}

// Lookup table for commands
const TelegramCommand commands[] = {
    { "state", cmd_state, "Report water level status" },
    { "uptime", cmd_uptime, "Report ESP32 uptime" },    
};
const int numCommands = sizeof(commands) / sizeof(TelegramCommand);
