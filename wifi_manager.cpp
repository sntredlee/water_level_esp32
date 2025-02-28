#include <WiFi.h>
#include "esp_wifi.h"
#include "wifi_manager.h"

static bool is_wifi_on = false;

bool is_wifi_on(){
  return is_wifi_on;
}

bool turn_on_wifi(String ssid, String passowrd) {
  // return true: wifi turned on successfully
  //        false: wifi turn on failed
#if DEBUG
  Serial.println("Connecting to WiFi...");
  Serial.flush();
#endif

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str());

  unsigned long startAttemptTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 500) {  // 0.5 timeout
    delay(50);
#if DEBUG
    Serial.print(".");
    Serial.flush();
#endif
  }

  if (WiFi.status() == WL_CONNECTED) {
    is_wifi_on = true;

#if DEBUG
    Serial.println("\nWiFi Connected!");
    Serial.flush();
#endif

  } else {

    is_wifi_on = false;

#if DEBUG
    Serial.println("WiFi Connection Failed.");
    Serial.flush();
#endif

  }

  return is_wifi_on;
}


void turn_off_wifi() {
  if (is_wifi_on) {
    // Disable WiFi to save power
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    is_wifi_on = false;

#if DEBUG
    Serial.println("WiFi turned Off.");
    Serial.flush();
#endif

  }
}
