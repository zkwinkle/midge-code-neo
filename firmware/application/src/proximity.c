#include "proximity.h"
#include "midge_protocol.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "time_control.h"
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/clock.h>

#include "storage.h"

LOG_MODULE_REGISTER(mb_proximity);

static struct bt_le_scan_param scan_param = {
    .type = BT_LE_SCAN_TYPE_PASSIVE,
    .options = BT_LE_SCAN_OPT_NONE,
    .interval = 0x0010,
    .window = 0x0010,
};

struct proximity_sensor_entry {
    union {
        uint8_t u8;
        int8_t i8;
    } rssi;
    uint8_t mac_address[6];
    struct CustomAdvertisementData advertised_data;
    uint64_t timestamp;
};

#define BUFFERED_SAMPLES 128

static struct {
    enum {
        PROXIMITY_SENSOR_STATE_DISABLED = 0,
        PROXIMITY_SENSOR_STATE_ACTIVE = 1,
        PROXIMITY_SENSOR_STATE_STOP = 2,
        PROXIMITY_SENSOR_STATE_ERR = 3,
    } state;
    int sample_cnt;
    struct proximity_sensor_entry buffered_samples[BUFFERED_SAMPLES];
} sensor_data = {
    .state = PROXIMITY_SENSOR_STATE_DISABLED,
    .sample_cnt = 0,
};

int proximity_sensor_init() {
    // check bt already init
    if (sensor_data.state != PROXIMITY_SENSOR_STATE_DISABLED) {
        LOG_INF("already initialized");
        return -EPERM;
    }
    sensor_data.state = PROXIMITY_SENSOR_STATE_ACTIVE;
    return 0;
}

uint8_t proximity_sensor_get_status(){
    return sensor_data.state;
}

int proximity_sensor_change_config(uint16_t interval, uint16_t window) {
    scan_param.interval = interval;
    scan_param.window = window;
    return 0;
}

static bool scan_data_parse(struct bt_data* data, void* advertised_data){
    if (data->type == BT_DATA_MANUFACTURER_DATA && data->data_len == sizeof(struct CustomAdvertisementData)) {
        memcpy(advertised_data, data->data, sizeof(struct CustomAdvertisementData));
        return false; // stop parsing
    }else{
        return true; // continue parsing
    }
}

K_MUTEX_DEFINE(proximity_sensor_mutex);
void scan_callback(const bt_addr_le_t* addr, int8_t rssi, uint8_t adv_type,
                   struct net_buf_simple* buf) {
    if (k_mutex_lock(&proximity_sensor_mutex, K_MSEC(50)) == 0) {
        struct proximity_sensor_entry* sample =
            &sensor_data.buffered_samples[sensor_data.sample_cnt];
        sample->rssi.i8 = rssi;
        memcpy(sample->mac_address, addr->a.val, 6);
        // obtain the advertised data
        bt_data_parse(buf, scan_data_parse, &sample->advertised_data);
        sample->timestamp = time_control_get_timestamp();
        sensor_data.sample_cnt++;
        if (sensor_data.sample_cnt == BUFFERED_SAMPLES) {
            sensor_data.sample_cnt = 0;
            storage_write(FILE_TYPE_PROXIMITY, sensor_data.buffered_samples,
                          sizeof(struct proximity_sensor_entry) * BUFFERED_SAMPLES);
        }
        k_mutex_unlock(&proximity_sensor_mutex);
    } else {
        LOG_ERR("failed to add sample");
    }
}

int proximity_sensor_start(int sample_iter) {
    // open new file
    storage_init_sample_file(FILE_TYPE_PROXIMITY, sample_iter);

    // start scan
    int ret = bt_le_scan_start(&scan_param, scan_callback);
    sensor_data.state = PROXIMITY_SENSOR_STATE_ACTIVE;
    sensor_data.sample_cnt = 0;
    return ret;
}

int proximity_sensor_stop() {
    int ret = bt_le_scan_stop();
    // write remaining data
    if (sensor_data.sample_cnt != 0) {
        storage_write(FILE_TYPE_PROXIMITY, sensor_data.buffered_samples,
                      sizeof(struct proximity_sensor_entry) * sensor_data.sample_cnt);
    }
    // close file
    storage_close(FILE_TYPE_PROXIMITY);
    sensor_data.state = PROXIMITY_SENSOR_STATE_STOP;
    return ret;
}
