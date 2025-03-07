#include <WiFi.h>
#include "esp_wifi.h"
#include "wifi_manager.h"
#include "config.h"

static bool _wifi_on = false;
static String ssid;
static String password;


bool is_wifi_on(){
  _wifi_on = (WiFi.status() == WL_CONNECTED);
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
  Serial.printf("Connecting to WiFi %s...\n\r", ssid.c_str());
  Serial.flush();
#endif

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  unsigned long startAttemptTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 5000) {  // 5.0 sec  timeout
    delay(500);
    vTaskDelay(1); // Not sure, put it here anyway
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