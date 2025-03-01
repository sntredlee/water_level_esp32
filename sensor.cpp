#include "sensor.h"
#include "sd_card.h"
#include "time.h"

static RTC_DATA_ATTR int8_t last_water_level = -1;
static RTC_DATA_ATTR int8_t curr_water_level = -1;
static RTC_DATA_ATTR uint32_t loops_since_last_2to1 = 0;
static RTC_DATA_ATTR uint32_t last_pump_op_time_in_loops = 0;
static RTC_DATA_ATTR uint64_t loops_since_last_log = 0;

static uint8_t loop_period_sec;
static uint8_t max_sec_between_logs;

void init_sensor(uint8_t _loop_period_sec, uint8_t _max_sec_between_logs){
  pinMode(32, OUTPUT);
  digitalWrite(32, LOW);

  pinMode(33, INPUT_PULLUP);
  pinMode(25, INPUT_PULLUP);
  pinMode(26, INPUT_PULLUP);
  pinMode(27, INPUT_PULLUP);
  pinMode(14, INPUT_PULLUP);

  loop_period_sec = _loop_period_sec;
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
  struct tm timeinfo;
  static char buffer[64];  // Buffer for formatted string
  static char timeString[30];

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

  getLocalTime(&timeinfo);
  strftime(timeString, sizeof(timeString), "%Y-%m-%d %H:%M:%S", &timeinfo);
  snprintf(buffer, sizeof(buffer), "%s--%d\n", timeString, curr_water_level);

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


int8_t check_water_level(){
  // This function should called once in every loop
  // read the latest water level, and handle the other tasks (analysis, logging)
  // return the latest water level
  curr_water_level = read_current_water_level();

  if ((curr_water_level != last_water_level) || (loops_since_last_log > max_sec_between_logs / loop_period_sec)) {    
    log_water_level(curr_water_level);
    loops_since_last_log = 0;
  } else {
    loops_since_last_log++;
  }

  if (curr_water_level >= last_water_level) {
    // no change, or increasing
    loops_since_last_2to1 += 1;    
  }else if ((curr_water_level == 1) && (last_water_level == 2)) {
    // Level 2 -> 1, pump operating
    loops_since_last_2to1 = 0;
  } else if ((curr_water_level == 0) && (last_water_level == 1)) {
    // Level 1 -> 0, pump operating
    last_pump_op_time_in_loops = loops_since_last_2to1;
  }

  last_water_level = curr_water_level;
  return curr_water_level;  
}