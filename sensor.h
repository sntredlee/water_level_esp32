#ifndef SENSOR_H
#define SENSOR_H

void init_sensor(uint8_t _loop_period_sec, uint8_t _max_sec_between_logs);
int8_t check_water_level();

#endif
