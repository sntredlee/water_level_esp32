#ifndef GLOBAL_CONFIG_H
#define GLOBAL_CONFIG_H
#include <Arduino.h>

#define VERSION_STRING "0.1"
#define DEBUG 1

typedef struct config{
  uint8_t loop_period_sec = 1;        // the period of each cycle
  uint32_t max_sec_between_logs = 10;  // log water level at least every x seconds
  uint32_t max_sec_between_comm = 15;  // connect to WiFi to check for and handle remote command at least every x seconds
  uint32_t max_sec_between_ntp = 7 * 24 * 3600; // do NTP sync at least every x seconds
  String ssid = "OPTUS_A2EE60N";     // WiFi SSID OPTUSVD3644ED0
  String password = "mohur52963mh";  // WiFi password   ALGAEBOMBS08954
  String telegram_botToken = "7646712534:AAH-FfQ82DJTWUbYglw2rOtWhhjGo2zclFM";    // **Telegram Bot Credentials**
  String telegram_chatID = "7108046311";                                          // **Telegram Bot Credentials**
}config_t;

#endif