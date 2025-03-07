#include <WiFi.h>
#include "wifi_manager.h"
#include "charge_controller.h"
#include "config.h"

// Data to send (8 bytes)
static uint8_t data_to_send[] = {0x06, 0x12, 0x10, 0x00, 0x00, 0x1d, 0x7c, 0xb7};
static uint8_t data_rx[80];  // 67 bytes are expected to be received

static String serverIP;
static uint16_t serverPort;

void config_charge_controller_addr(String ip, uint16_t port){
  serverIP = ip;
  serverPort = port;
}


cmd_err_t battery_volt_command(int argc, char *argv[]){
  if (!is_wifi_on() && !turn_on_wifi()) {
    console_printf("Can NOT connect to WiFi.\n");
    return ERR_UNKNOWN;
  }  
  // WiFi Client
  WiFiClient client;
  if (client.connect(serverIP.c_str(), serverPort)) {
#if DEBUG
      Serial.printf("Connected to charge controller TCP server.\n\r");
      Serial.flush();
#endif
      // Send data
      client.write(data_to_send, sizeof(data_to_send));

      // Receive response
      delay(1000); // Wait for response
      int i = 0;
      while (client.available()) {
          uint8_t receivedByte = client.read();
          data_rx[i++] = receivedByte;
      }
#if DEBUG
      Serial.printf("Received %d bytes response from charge controller.\n\r", i);
      Serial.flush();
#endif    
      uint8_t voltage = data_rx[8];  // voltage x 10
      console_printf("Battery Voltage: %d.%d V", voltage / 10, voltage % 10);

      // Close connection
      client.stop();
      return ERR_CMD_OK; 
  } else {
      console_printf("Failed to connect to server!");
      return ERR_UNKNOWN;
  }
}
