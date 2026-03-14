#include "imu.h"

#include <errno.h>
#include <zephyr/logging/log.h>

#include "ICM20948_driver_interface.h"
#include "storage.h"

LOG_MODULE_REGISTER(imu);



uint8_t imu_sensor_get_status(){
    return 0;
}

int imu_sensor_init() {
    return icm20948_init();
}

int imu_sensor_start(int sample_iter) {
    // start imu files
    int accel_ret = storage_init_sample_file(FILE_TYPE_ACCEL, sample_iter);
    int gyro_ret = storage_init_sample_file(FILE_TYPE_GYRO, sample_iter);
    int magneto_ret = storage_init_sample_file(FILE_TYPE_MAGNETO, sample_iter);
    int stat = (accel_ret != 0) || (gyro_ret != 0) || (magneto_ret != 0);
    if (stat) {
        LOG_ERR("Error initializing IMU sample files acc: %d - gyro: %d - magneto: %d", accel_ret,
                gyro_ret, magneto_ret);
        if (accel_ret == 0) {
            storage_close(FILE_TYPE_ACCEL);
        }
        if (gyro_ret == 0) {
            storage_close(FILE_TYPE_GYRO);
        }
        if (magneto_ret == 0) {
            storage_close(FILE_TYPE_MAGNETO);
        }
        return -EAGAIN;
    }
    // start sensors
    return icm20948_enable_sensors();
}

int imu_sensor_stop() {
    // stop sensors
    int ret = icm20948_disable_sensors();
    // close imu files
    if (ret == 0) {
        storage_close(FILE_TYPE_ACCEL);
        storage_close(FILE_TYPE_GYRO);
        storage_close(FILE_TYPE_MAGNETO);
    }
    return ret;
}
