#include <WiFi.h>
#include "esp_wifi.h"
#include "wifi_manager.h"
#include "config.h"

static bool _wifi_on = false;
static String ssid;
static String password;


bool is_wifi_on(){
  return _wifi_on;
}

void init_wifi(String _ssid, String _password){
  ssid = _ssid;
  password = _password;
}


bool turn_on_wifi() {
  // return true: wifi turned on successfully
  //        false: wifi turn on failed
#if DEBUG
  Serial.println("Connecting to WiFi...");
  Serial.flush();
#endif

  WiFi.mode(WIFI_STA);
  WiFi.begin();

  unsigned long startAttemptTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 500) {  // 0.5 timeout
    delay(50);
#if DEBUG
    Serial.print(".");
    Serial.flush();
#endif
  }

  if (WiFi.status() == WL_CONNECTED) {
    _wifi_on = true;

#if DEBUG
    Serial.println("\nWiFi Connected!");
    Serial.flush();
#endif

  } else {

    _wifi_on = false;

#if DEBUG
    Serial.println("WiFi Connection Failed.");
    Serial.flush();
#endif

  }

  return _wifi_on;
}


void turn_off_wifi() {
  if (is_wifi_on) {
    // Disable WiFi to save power
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    _wifi_on = false;

#if DEBUG
    Serial.println("WiFi turned Off.");
    Serial.flush();
#endif

  }
}
