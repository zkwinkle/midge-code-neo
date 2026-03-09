#include "battery_charge.h"

#include <zephyr/drivers/adc.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(mb_battery);

#define VOLTAGE_DIVIDER_RATIO 2

static const struct adc_dt_spec adc_channel = ADC_DT_SPEC_GET(DT_PATH(zephyr_user));
static int32_t buf = 0;

static struct adc_sequence sequence = {
    .buffer = &buf,
    /* buffer size in bytes, not number of samples */
    .buffer_size = sizeof(int32_t),
    // Optional
    //.calibrate = true,
};

int battery_charge_init() {
    int ret;
    if (!adc_is_ready_dt(&adc_channel)) {
        LOG_ERR("ADC %s not ready", adc_channel.dev->name);
        return 0;
    }
    ret = adc_channel_setup_dt(&adc_channel);
    if (ret < 0) {
        LOG_ERR("Could not setup channel #%d", ret);
        return 0;
    }

    ret = adc_sequence_init_dt(&adc_channel, &sequence);
    if (ret < 0) {
        LOG_ERR("Could not initalize sequence");
    }
    LOG_INF("init OK");
    return ret;
}

int battery_charge_get_mv(int16_t* val_mv) {
    int ret;

    ret = adc_read(adc_channel.dev, &sequence);
    if (ret < 0) {
        LOG_ERR("Could not read (%d)", ret);
        return ret;
    }
    int32_t mv = buf;
    ret = adc_raw_to_millivolts_dt(&adc_channel, &mv);
    /* conversion to mV may not be supported, skip if not */
    if (ret < 0) {
        LOG_ERR(" (value in mV not available)");
    } else {
        // safe cast to cast to int16_t as battery voltage can be expected to be
        // below ~4200mV as it is a LiPO battery
        *val_mv = (int16_t)(mv * VOLTAGE_DIVIDER_RATIO);
        LOG_DBG("val = %d mV  buf: %d", *val_mv, buf);
    }
    return ret;
}
