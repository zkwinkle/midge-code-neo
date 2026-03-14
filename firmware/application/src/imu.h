#ifndef MBFW_IMU_H
#define MBFW_IMU_H


#include "ICM20948_driver_interface.h"

#include <inttypes.h>

uint8_t imu_sensor_get_status();

int imu_sensor_init();

int imu_sensor_start(int sample_iter);

int imu_sensor_stop();


#endif //MBFW_IMU_H
