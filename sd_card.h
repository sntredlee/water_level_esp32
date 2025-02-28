#ifndef SD_CARD_H
#define SD_CARD_H

#include <FS.h>
#include <SD.h>
#include <SPI.h>
#include "config.h"


#define SD_CS 4

bool init_sd_card();
config_t readConfigFile();
bool writeFile(const char *path, const char *message);
bool appendFile(const char *path, const char *message);

#endif