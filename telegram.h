#ifndef TELEGRAM_H
#define TELEGRAM_H
#include <Arduino.h>

void init_telegram(String telegram_botToken, String telegram_chatID);
bool sendTelegramMessage(const char *format, ...);
void process_telegram_messages();

#endif