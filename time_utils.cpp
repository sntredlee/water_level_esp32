#include "wifi_manager.h"
#include "time.h"
#include "time_utils.h"

// **NTP Server for RTC Sync**
static const char *ntpServer = "pool.ntp.org";
static const long gmtOffset_sec = 36000 + 3600;  // GMT + 11:00 with daylight saving , Sydney AU
static const long daylightOffset_sec = 3600;

// **Initialize RTC Time**
static struct tm timeinfo;

// **Format the timestamp**
static char timeString[30];

void sync_clock(){
  if (!is_wifi_on() && !turn_on_wifi()) {
    return;
  }
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  return;
}

const char* get_current_time(){
  getLocalTime(&timeinfo);
  strftime(timeString, sizeof(timeString), "%Y-%m-%d %H:%M:%S", &timeinfo);
  return timeString;
}