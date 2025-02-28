#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include "telegram.h"
#include "wifi_manager.h"


static WiFiClientSecure secure_client;
static UniversalTelegramBot *bot = nullptr;
static String chat_id;


void handle_command(String text) {
    text.trim();
    
    if (text.equalsIgnoreCase("state")) {
        Serial.println("Water Level State Requested");
    } else if (text.equalsIgnoreCase("uptime")) {
        Serial.println("System Uptime Requested");
    } else {
        Serial.println("Unknown Command");
    }
}


void init_telegram(String telegram_botToken, String telegram_chatID){  
  chat_id = telegram_chatID;
  secure_client.setInsecure();
  bot = new UniversalTelegramBot(telegram_botToken.c_str(), secure_client);
  sendTelegramMessage("I am online.");
}

int process_telegram_messages() {
  int num_mesgs_total = 0;
  if (!is_wifi_on() && !turn_on_wifi()) {
    return 0;
  }

  int messageCount = bot->getUpdates(bot->last_message_received + 1);  
#if DEBUG
  Serial.printf("message count: %d\n", messageCount);
#endif

  vTaskDelay(1); // make watchdog happy, bot->getUpdates() may take a long time

  while (messageCount) {

#if DEBUG
    Serial.println("New Telegram Message Received!");
    Serial.flush();
#endif

    for (int i = 0; i < messageCount; i++) {
      String text = bot->messages[i].text;
      // String sender = bot->messages[i].from_name;
#if DEBUG
      Serial.print("Message: ");            
      Serial.println(text);
      Serial.flush();
#endif

      handle_command(text);
      vTaskDelay(1);
    }
    num_mesgs_total += messageCount;

    messageCount = bot->getUpdates(bot->last_message_received + 1);
  }  
  return num_mesgs_total;
}


bool sendTelegramMessage(const char *format, ...) {
  // return: true - success , false - failure

  if (!is_wifi_on() && !turn_on_wifi()) {
    return false;
  }

  // **Format the message using snprintf()**
  char message[256];  // Buffer for formatted message
  va_list args;
  va_start(args, format);
  vsnprintf(message, sizeof(message), format, args);
  va_end(args);

#if DEBUG
  Serial.printf("Send %s to Telegram.", message);
  Serial.flush();
#endif

  return bot->sendMessage(telegram_chatID, message, "");
}