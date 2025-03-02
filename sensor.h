#ifndef SENSOR_H
#define SENSOR_H

#include <stdint.h>

void init_sensor(uint8_t _loop_period_sec, uint8_t _max_sec_between_logs);
void check_water_level();

#endif
