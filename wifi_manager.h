#ifndef WIFI_MAMAGER_H
#define WIFI_MAMAGER_H

bool is_wifi_on();
void init_wifi(String ssid, String passowrd);
bool turn_on_wifi();
void turn_off_wifi();

#endif