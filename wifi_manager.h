#ifndef WIFI_MAMAGER_H
#define WIFI_MAMAGER_H

#include "config.h"

bool is_wifi_on();
bool turn_on_wifi(String ssid, String passowrd);
void turn_off_wifi();

#endif