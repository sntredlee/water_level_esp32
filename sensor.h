#ifndef SENSOR_H
#define SENSOR_H

#include <Arduino.h>
#include <stdint.h>
#include "command.h"

void init_sensor(uint32_t _max_sec_between_logs);
void check_water_level();
cmd_err_t state_command(int argc, char *argv[]);
cmd_err_t notify_pump_op_command(int argc, char *argv[]);
cmd_err_t pump_op_time_threshold_command(int argc, char *argv[]);
cmd_err_t water_level_threshold_command(int argc, char *argv[]);
#endif
