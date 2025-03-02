#ifndef TELEGRAM_H
#define TELEGRAM_H

void init_telegram(String telegram_botToken, String telegram_chatID);
bool sendTelegramMessage(const char *format, ...);
int process_telegram_messages();

#endif