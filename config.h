#ifndef GLOBAL_CONFIG_H
#define GLOBAL_CONFIG_H
#include <Arduino.h>

#define VERSION_STRING "0.1"
#define DEBUG 1

typedef struct config{
  uint8_t low_power_mode = 1;        // is running low power mode? 1/0, WiFi is always ON when lower power mode is off
  uint8_t loop_period_sec = 2;          // the period of each cycle
  uint32_t max_sec_between_logs = 600;  // log water level at least every x seconds
  uint32_t max_sec_between_comm = 30;   // connect to WiFi to check for and handle remote command at least every x seconds
  uint32_t max_sec_between_ntp = 24 * 3600; // do NTP sync at least every x seconds
  String ssid = "OPTUS_A2EE60N";     // WiFi SSID OPTUSVD3644ED0
  String password = "mohur52963mh";  // WiFi password   ALGAEBOMBS08954
  String telegram_botToken = "7646712534:AAH-FfQ82DJTWUbYglw2rOtWhhjGo2zclFM";    // **Telegram Bot Credentials**
  String telegram_chatID = "7108046311";                                          // **Telegram Bot Credentials**
  // Wind-Solar Charging Controller, RS485-WiFi communication module
  String charge_controller_ip = "192.168.0.6";
  uint16_t charge_controller_port = 8899;
}config_t;

#endif