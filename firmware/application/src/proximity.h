#ifndef MBFW_PROXIMITY_H
#define MBFW_PROXIMITY_H

#include <inttypes.h>

int proximity_sensor_init();

uint8_t proximity_sensor_get_status();

int proximity_sensor_change_config(uint16_t interval, uint16_t window);

int proximity_sensor_start(int sample_iter);

int proximity_sensor_stop();

#endif
