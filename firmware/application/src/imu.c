#include "imu.h"

#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include "midge_protocol.h"
#include "storage.h"
#include "utility.h"

#if CONFIG_MIDGE_CODE_IMU_ICM20948_USE_CLOSED_DRIVER
#include "ICM20948_driver_interface.h"
#else
#error "No IMU driver defined"
#endif


LOG_MODULE_REGISTER(imu);

uint8_t imu_sensor_get_status() { return 0; }

int imu_sensor_init() { return icm20948_init(); }

int imu_sensor_start(int sample_iter, uint16_t acc_fsr, uint16_t gyr_fsr, uint16_t datarate) {
    // start imu files
    int accel_ret = storage_init_sample_file(FILE_TYPE_ACCEL, sample_iter);
    int gyro_ret = storage_init_sample_file(FILE_TYPE_GYRO, sample_iter);
    int magneto_ret = storage_init_sample_file(FILE_TYPE_MAGNETO, sample_iter);
    int rot_ret = storage_init_sample_file(FILE_TYPE_ROTATION, sample_iter);
    int stat = (accel_ret != 0) || (gyro_ret != 0) || (magneto_ret != 0) || (rot_ret != 0);
    if (stat) {
        LOG_ERR("Error initializing IMU sample files acc: %d - gyro: %d - magneto: %d - rot: %d",
                accel_ret, gyro_ret, magneto_ret, rot_ret);
        if (accel_ret == 0) {
            storage_close(FILE_TYPE_ACCEL);
        }
        if (gyro_ret == 0) {
            storage_close(FILE_TYPE_GYRO);
        }
        if (magneto_ret == 0) {
            storage_close(FILE_TYPE_MAGNETO);
        }
        if (rot_ret == 0) {
            storage_close(FILE_TYPE_ROTATION);
        }
        return -EAGAIN;
    }
    // start sensors
    int ret = icm20948_set_fsr((uint32_t)acc_fsr, (uint32_t)gyr_fsr);
    if (ret != 0) {
        return ret;
    }

    ret = icm20948_set_datarate(datarate);
    if (ret != 0) {
        return ret;
    }

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
        storage_close(FILE_TYPE_ROTATION);
    }
    return ret;
}

struct StartImuWorkCtx {
    struct k_work work;
    struct k_sem done;
    uint16_t sample_id;
    uint16_t acc_fsr;
    uint16_t gyr_fsr;
    uint16_t datarate;
    int ret;
};

static void start_imu_work_handler(struct k_work* work) {
    struct StartImuWorkCtx* ctx = CONTAINER_OF(work, struct StartImuWorkCtx, work);
    ctx->ret = imu_sensor_start(ctx->sample_id, ctx->acc_fsr, ctx->gyr_fsr, ctx->datarate);
    k_sem_give(&ctx->done);
}

int cmd_start_imu(uint8_t* data) {
    struct CmdStartIMURequest* req_data = (struct CmdStartIMURequest*)data;
    struct StartImuWorkCtx ctx = {.sample_id = req_data->sample_id,
                                  .acc_fsr = req_data->acc_fsr,
                                  .gyr_fsr = req_data->gyr_fsr,
                                  .datarate = req_data->datarate};
    k_sem_init(&ctx.done, 0, 1);
    k_work_init(&ctx.work, start_imu_work_handler);
    int ret = k_work_submit(&ctx.work);
    if (ret < 0) {
        LOG_ERR("Failed to submit start imu work: %d", ret);
    } else {
        ret = k_sem_take(&ctx.done, K_FOREVER);
        if (ret != 0) {
            LOG_ERR("Failed to take semaphore for start imu work: %d", ret);
        } else {
            ret = ctx.ret;
        }
    }

    struct CmdStartIMUResponse* resp_data = (struct CmdStartIMUResponse*)data;
    memset(resp_data, 0, sizeof(struct CmdStartIMUResponse));
    resp_data->status_code = ret;
    return ret;
}

static void stop_imu_work_handler(struct k_work* work) {
    struct SimpleWorkCtx* ctx = CONTAINER_OF(work, struct SimpleWorkCtx, work);
    ctx->ret = imu_sensor_stop();
    k_sem_give(&ctx->done);
}

int cmd_stop_imu(uint8_t* data) {
    // struct CmdStopIMURequest* req_data = (struct CmdStopIMURequest*)data;
    struct CmdStopIMUResponse* resp_data = (struct CmdStopIMUResponse*)data;
    struct SimpleWorkCtx ctx;
    k_work_init(&ctx.work, stop_imu_work_handler);
    k_sem_init(&ctx.done, 0, 1);

    int ret;
    int submit_ret = k_work_submit(&ctx.work);
    if (submit_ret < 0) {
        ret = submit_ret;
    } else {
        k_sem_take(&ctx.done, K_FOREVER);
        ret = ctx.ret;
    }
    resp_data->status_code = ret;
    return ret;
}
