#include <Arduino.h>
#include "config.h"
#include "sensor.h"
#include "sd_card.h"
#include "time.h"
#include "telegram.h"
#include "time_utils.h"

#define WARN_PIN 15
static bool warning_mesg_sent;

static int8_t last_water_level = -1;
static int8_t curr_water_level = -1;
static uint64_t ts_us_last_2to1 = 0;
static int last_pump_op_time_sec = 0;
static uint64_t ts_us_last_log = 0;

static uint32_t max_sec_between_logs;

void init_sensor(uint32_t _max_sec_between_logs){
  pinMode(32, OUTPUT);
  digitalWrite(32, LOW);

  pinMode(33, INPUT_PULLUP);
  pinMode(25, INPUT_PULLUP);
  pinMode(26, INPUT_PULLUP);
  pinMode(27, INPUT_PULLUP);
  pinMode(14, INPUT_PULLUP);

  digitalWrite(WARN_PIN, LOW);
  pinMode(WARN_PIN, OUTPUT);

  max_sec_between_logs = _max_sec_between_logs;
}

static int8_t read_current_water_level() {
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
  // digitalWrite(32, LOW);
  if (!digitalRead(14)) {
    lvl = 5;
  } else if (!digitalRead(27)) {
    lvl = 4;
  } else if (!digitalRead(26)) {
    lvl = 3;
  } else if (!digitalRead(25)) {
    lvl = 2;
  } else if (!digitalRead(33)) {
    lvl = 1;
  }
  // digitalWrite(32, LOW);

  return lvl;
}


void log_water_level(int8_t latest_water_lvl) {  
  static char buffer[64];  // Buffer for formatted string

  if (!is_sd_card_ok()) {
#if DEBUG
    Serial.printf("SD card not OK, skip logging.\n");
    Serial.flush();
#endif
    return;
  }

#if DEBUG
  Serial.printf("SD card OK, logging.\n");
  Serial.flush();
#endif

  snprintf(buffer, sizeof(buffer), "%s--%d\n", get_current_time(), curr_water_level);

#if DEBUG
  Serial.printf("log to write: %s\n", buffer);
#endif

  if (!appendFile("/water_level_log.txt", buffer)) {

#if DEBUG
    Serial.printf("Appending failed, try writing..\n");
    Serial.flush();
#endif

    writeFile("/water_level_log.txt", buffer);
  }
}


void check_water_level(){
  // This function should called once in every loop
  // read the latest water level, and handle the other tasks (analysis, logging)
  curr_water_level = read_current_water_level();
  uint64_t curr_ts_us = esp_timer_get_time();
  if ((curr_water_level != last_water_level) || ((curr_ts_us - ts_us_last_log)/1000000 > max_sec_between_logs)) {    
    log_water_level(curr_water_level);
    ts_us_last_log = curr_ts_us;
  }

  if (curr_water_level >= last_water_level) {
    // no change, or increasing
        
  }else if ((curr_water_level == 1) && (last_water_level == 2)) {
    // Level 2 -> 1, pump operating
    ts_us_last_2to1 = curr_ts_us;
#if DEBUG
    Serial.printf("Water level 2->1. t21=%llu\n", ts_us_last_2to1);
    Serial.flush();
#endif
  } else if ((curr_water_level == 0) && (last_water_level == 1)) {
    // Level 1 -> 0, pump operating
    last_pump_op_time_sec = (curr_ts_us - ts_us_last_2to1)/1000000;
#if DEBUG
    Serial.printf("Water level 1->0. Time since 2->1: %d sec. t21=%llu, t10=%llu\n", last_pump_op_time_sec, ts_us_last_2to1, curr_ts_us);
    Serial.flush();
#endif
  }

  last_water_level = curr_water_level;

  // make watch dog happy, as this is a heavy task
  vTaskDelay(1);
    
  // Alarm ?
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
}


// Command function implementations
cmd_err_t state_command(int argc, char *argv[]){

  int t_sec = esp_timer_get_time() / 1000000;
  console_printf("Up %d days %02d:%02d:%02d.",
                  t_sec / 3600 / 24,
                  (t_sec / 3600) % 24,
                  (t_sec / 60) % 60,
                  t_sec % 60
                  );
  console_printf("Current time: %s, water level: %d\n", get_current_time(), curr_water_level);
  
  if (last_pump_op_time_sec != 0) {
      int seconds_since_pump_op = (esp_timer_get_time() - ts_us_last_2to1) / 1000000;
      console_printf("Pump last working %d days %02d:%02d:%02d ago, took %d sec",
                          seconds_since_pump_op / 3600 / 24,
                          (seconds_since_pump_op / 3600) % 24,
                          (seconds_since_pump_op / 60) % 60,
                          seconds_since_pump_op % 60,
                          last_pump_op_time_sec);
  }
  return ERR_CMD_OK;
}
