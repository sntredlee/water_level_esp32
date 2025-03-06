#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include "telegram.h"
#include "wifi_manager.h"
#include "config.h"
#include "command.h"

/*
Note about telegram:

ArduinoJson version 6.16.0  (Version 7 will not work!!!)
UniversalTelegramBot : version 1.3.0
*/

static WiFiClientSecure secure_client;
static UniversalTelegramBot *bot = nullptr;
static String chat_id;


static void handle_telegram_command(String text) {
    text.trim();
    console_parse_cmd(text.c_str());

}


void init_telegram(String telegram_botToken, String telegram_chatID){  
  chat_id = telegram_chatID;
  secure_client.setInsecure();
  bot = new UniversalTelegramBot(telegram_botToken.c_str(), secure_client);
  sendTelegramMessage("I am online.");
}

void process_telegram_messages() {
  int num_mesgs_total = 0;
  if (!is_wifi_on() && !turn_on_wifi()) {
    return;
  }

  int messageCount = bot->getUpdates(bot->last_message_received + 1);  
  vTaskDelay(1); // make watchdog happy, bot->getUpdates() may take a long time

  while (messageCount) {

    for (int i = 0; i < messageCount; i++) {
      String text = bot->messages[i].text;
      // String sender = bot->messages[i].from_name;
#if DEBUG
      Serial.print("Telegram message: ");            
      Serial.println(text);
      Serial.flush();
#endif

      // pass the string to command console
      text.trim();
      text.toLowerCase();
      text.replace('-', '_');
      console_parse_cmd(text.c_str());
      const char* cmd_output = get_command_output();
      if (strlen(cmd_output)){
        sendTelegramMessage(cmd_output);   
      }
    
      // make watch dog happy, as this is a heavy task
      vTaskDelay(1);
    }
    num_mesgs_total += messageCount;

    messageCount = bot->getUpdates(bot->last_message_received + 1);
  }  
#if DEBUG
  Serial.printf("Total telegram message processed: %d\n", num_mesgs_total);
#endif
}


bool sendTelegramMessage(const char *format, ...) {
  // return: true - success , false - failure

  if (!is_wifi_on() && !turn_on_wifi()) {
    return false;
  }

  // **Format the message using snprintf()**
  static char message[256];  // Buffer for formatted message
  va_list args;
  va_start(args, format);
  vsnprintf(message, sizeof(message), format, args);
  va_end(args);

#if DEBUG
  Serial.printf("Send %s to Telegram.", message);
  Serial.flush();
#endif

  return bot->sendMessage(chat_id, message, "");
}