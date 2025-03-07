#ifndef CHARGE_CONTROLLER_H
#define CHARGE_CONTROLLER_H

#include <Arduino.h>
#include "command.h"

void config_charge_controller_addr(String ip, uint16_t port);
cmd_err_t battery_volt_command(int argc, char *argv[]);

#endif