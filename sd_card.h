#ifndef SD_CARD_H
#define SD_CARD_H

#include <Arduino.h>
#include "config.h"

bool init_sd_card();
bool is_sd_card_ok();
config_t readConfigFile();
bool writeFile(const char *path, const char *message);
bool appendFile(const char *path, const char *message);

#endif