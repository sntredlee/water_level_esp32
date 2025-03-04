#include "time.h"
#include "config.h"
#include "command.h"
#include "sd_card.h"
#include "wifi_manager.h"
#include "telegram.h"
#include "sensor.h"
#include "time_utils.h"
/*
  // run into problem when the code crash for unknown reason.
  // thought it was stack overflow but it wasn't.
  // Eventually find out the problem following chatGPT to print the backtrace:
  // "C:\\Users\\Yun\\AppData\\Local\\Arduino15\\packages\\esp32\\tools\\esp-x32\\2405/bin/xtensa-esp32-elf-addr2line.exe" 
  //    -e "C:\Users\Yun\AppData\Local\arduino\sketches\B49BC885815448DCD104CD2F00AFED07\sketch_feb15a.ino.elf" 
  //     0x40089f8a:0x3ffb3cd0 0x40173c06:0x3ffb3ce0 0x4016e6f9:0x3ffb4000 0x4016e736:0x3ffb4090 0x400de96c:0x3ffb40d0 0x400de9fd:0x3ffb4150 0x400d3298:0x3ffb41a0 0x400d32f9:0x3ffb41f0 0x400d33a4:0x3ffb4210 0x400d33f9:0x3ffb4230 0x400d34ff:0x3ffb4250 0x400e1c54:0x3ffb4270 0x4008cf7e:0x3ffb4290
  // The path to .exe and .elf is found by Go to File > Preferences.
  //      Enable "Show Verbose Output" for:
  //            Compilation
  //            Upload
*/

#define LED_PIN 5
#define LED_ON LOW
#define LED_OFF HIGH

static config_t sys_config;

static const command_t init_commands[] =
{
    { "state",             state_command,        0, NULL, NULL, NULL,    "Get water level state"}, \    
    CMD_TABLE_END
};


// Periodic tasks
typedef void (*periodic_task_function_t)();
typedef struct{
  const char * task_name;
  periodic_task_function_t task_func;
  uint32_t task_period_sec;
  uint64_t last_run_ts;
}periodic_task_t;
#define MAX_PERIODIC_TASKS 10
static periodic_task_t periodic_tasks[MAX_PERIODIC_TASKS];
static int num_periodic_tasks = 0;


void setup() {
  
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LED_ON);

  Serial.begin(115200);
  init_cmd(init_commands);
 
  if (init_sd_card()){
    sys_config = readConfigFile();
  }

  init_wifi(sys_config.ssid, sys_config.password);
  
  sync_clock();

  init_telegram(sys_config.telegram_botToken, sys_config.telegram_chatID);

  init_sensor(sys_config.max_sec_between_logs);

  // setup tasks
  Serial.printf("Adding task for water level check.\n\r");
  periodic_task_t water_level_task{
    .task_name="water level check", 
    .task_func=check_water_level,
    .task_period_sec=sys_config.loop_period_sec,
    .last_run_ts=0
  };
  periodic_tasks[num_periodic_tasks++] = water_level_task;

  Serial.printf("Adding task for handling telegram messages.\n\r");
  periodic_task_t telegram_task{
    .task_name="telegram commands", 
    .task_func=process_telegram_messages,
    .task_period_sec=sys_config.max_sec_between_comm,
    .last_run_ts=0
  };
  periodic_tasks[num_periodic_tasks++] = telegram_task;
  
  Serial.printf("Adding task for synchronizing clock.\n\r");
  periodic_task_t ntp_task{
    .task_name="NTP", 
    .task_func=sync_clock,
    .task_period_sec=sys_config.max_sec_between_ntp,
    .last_run_ts=0
  };
  periodic_tasks[num_periodic_tasks++] = ntp_task;
  
  digitalWrite(LED_PIN, LED_OFF); 
}


void loop() {    
  
  uint64_t ts_loop_start = esp_timer_get_time();

  digitalWrite(LED_PIN, LED_ON);
  for (int i_task=0; i_task < num_periodic_tasks; i_task++){
      periodic_task_t* p_task = &periodic_tasks[i_task];
      if ((0 == p_task->last_run_ts) || (ts_loop_start >= p_task->last_run_ts + p_task->task_period_sec * 1000000)){
#if DEBUG
        Serial.printf("Running task %s\n\r", p_task->task_name);
        Serial.flush();
#endif
        p_task->task_func();
        p_task->last_run_ts = ts_loop_start;
      }
      vTaskDelay(1);
  }
  digitalWrite(LED_PIN, LED_OFF);

  // Prepare to go to sleep
  uint64_t loop_cost_us = esp_timer_get_time() - ts_loop_start;
#if DEBUG
  Serial.print("Loop time (us): ");
  Serial.println(loop_cost_us);
  Serial.flush();
#endif
  if (loop_cost_us >= sys_config.loop_period_sec * 1000000) {
    // This loop takes too long, no sleep, go to next loop immediately
    // delay(1 ms) is called for the backend to do something.
    delay(1);
  } else {
    // light sleep is more power efficient because we need to wake up frequently and the power cost of rebooting (150ms @ 200mA) is much more
    turn_off_wifi();
#if DEBUG
    Serial.print("go to light sleep mode...");
    Serial.flush();
#endif
    esp_sleep_enable_timer_wakeup(sys_config.loop_period_sec * 1000000 - loop_cost_us);
    esp_light_sleep_start();
  }
}