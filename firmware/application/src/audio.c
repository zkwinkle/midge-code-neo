#include "audio.h"

#include <stdbool.h>
#include <string.h>
#include <zephyr/audio/dmic.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "privacy_switch.h"
#include "storage.h"

LOG_MODULE_REGISTER(mb_audio);

#define MAX_SAMPLE_RATE 20000

#define SAMPLE_BIT_WIDTH 16
#define BYTES_PER_SAMPLE sizeof(int16_t)

/* Size of a block for 100 ms of audio data. */
#define BLOCK_SIZE(_sample_rate, _number_of_channels) \
    (BYTES_PER_SAMPLE * (_sample_rate / 10) * _number_of_channels)

/* Driver will allocate blocks from this slab to receive audio data into them.
 * Application, after getting a given block from the driver and processing its
 * data, needs to free that block.
 */
#define MAX_BLOCK_SIZE (4096)  // BLOCK_SIZE(MAX_SAMPLE_RATE, 1)
#define BLOCK_COUNT 3

K_MEM_SLAB_DEFINE_STATIC(mem_slab, MAX_BLOCK_SIZE, BLOCK_COUNT, 4);

#define HIGH_SAMPLE_RATE MAX_SAMPLE_RATE
#define LOW_SAMPLE_RATE_DECIMATION 16
#define LOW_SAMPLE_RATE (HIGH_SAMPLE_RATE / LOW_SAMPLE_RATE_DECIMATION)

const struct device* const dmic_dev = DEVICE_DT_GET(DT_NODELABEL(dmic_dev));
/**
 * @brief Microphone configuration. Depends on hardware. Might be useful to
 * have the config defined as part of the device tree
 * For now, assumes usage of PDM Mic ST MP34DT05TR-A
 */
struct pdm_io_cfg microphone_cfg = {
    .min_pdm_clk_freq = 1200000,
    .max_pdm_clk_freq = 3250000,
    // usual value in datasheets: 40% min, 60% max, 50% typical
    .min_pdm_clk_dc = 40,
    .max_pdm_clk_dc = 60,
};

struct pcm_stream_cfg stream = {
    .pcm_width = SAMPLE_BIT_WIDTH,
    .mem_slab = &mem_slab,
};

static struct {
    enum {
        AUDIO_SENSOR_STATE_DISABLED = 0,
        AUDIO_SENSOR_STATE_ACTIVE = 1,
        AUDIO_SENSOR_STATE_STOP = 2,
        AUDIO_SENSOR_STATE_ERR = 3,
    } state;
    struct dmic_cfg audio_config;
    int sample_iter;
    bool stopped;
} sensor_data = {.state = AUDIO_SENSOR_STATE_DISABLED, .sample_iter = 0, .audio_config = {}};

uint8_t audio_sensor_get_status() { return (uint8_t)sensor_data.state; }

K_THREAD_STACK_DEFINE(audio_sampling_thread_stack, 2048);
struct k_thread audio_sampling_thread_data;
void audio_sampling_thread(void* a0, void* a1, void* a2) {
    int status = 0;
    status = storage_init_sample_file(FILE_TYPE_AUDIO, sensor_data.sample_iter);
    if (status < 0) {
        LOG_ERR("Failed to open sampling file, status %d", status);
        sensor_data.state = AUDIO_SENSOR_STATE_ERR;
        return;
    }
    void* audio_buffer;
    uint32_t audio_buffer_size;

    // trigger start
    status = dmic_trigger(dmic_dev, DMIC_TRIGGER_START);
    if (status < 0) {
        // error on trigger
        LOG_ERR("trigger start");
        sensor_data.state = AUDIO_SENSOR_STATE_ERR;
        storage_close(FILE_TYPE_AUDIO);
        return;
    }

    // read
    int sample_size_ms = (sensor_data.audio_config.streams->block_size * 1000) /
                         (sensor_data.audio_config.streams->pcm_rate *
                          sensor_data.audio_config.channel.act_num_chan);
    LOG_INF("starting audio sampling");
    while (sensor_data.state == AUDIO_SENSOR_STATE_ACTIVE) {
        // read buffer
        status = dmic_read(dmic_dev, 0, &audio_buffer, &audio_buffer_size, 0);
        if (status < 0) {
            // error on trigger
            if (status == -EBUSY || status == -ENOMSG) {
                // LOG_INF("read: no data available");
            } else {
                LOG_ERR("read %d\n, sample probably dropped", status);
            }
            k_yield();
            //k_msleep(sample_size_ms);
            continue;
        }
        //  write to file
        status = storage_write(FILE_TYPE_AUDIO, audio_buffer, audio_buffer_size);
        // free the buffer
        k_mem_slab_free(&mem_slab, audio_buffer);
        k_yield();
        //k_msleep(sample_size_ms);
        if (status < 0) {
            LOG_ERR("write\n");
            break;
        }
    }

    // trigger stop
    status = dmic_trigger(dmic_dev, DMIC_TRIGGER_STOP);
    if (status < 0) {
        // error on trigger
        LOG_ERR("trigger stop\n");
    }
    LOG_INF("finished sampling round");
    storage_close(FILE_TYPE_AUDIO);
}

int audio_sensor_start(int sample_iter, int mode) {
    int ret;
    // perdir archivo

    // Obtener frecuencia de sampling a partir del switch
    enum privacy_sw_pos switch_pos = switch_sensor_position();

    // configurar canales
    memset(&sensor_data.audio_config, 0,
           sizeof(struct dmic_cfg));  // clear configuration

    sensor_data.audio_config.channel.req_num_streams = 1;
    sensor_data.audio_config.io = microphone_cfg;

    sensor_data.audio_config.channel.req_num_chan = mode;
    switch (mode) {
        case AUDIO_MODE_MONO: {
            sensor_data.audio_config.channel.req_chan_map_lo =
                dmic_build_channel_map(0, 0, PDM_CHAN_LEFT);
        } break;
        case AUDIO_MODE_STEREO: {
            sensor_data.audio_config.channel.req_chan_map_lo =
                dmic_build_channel_map(0, 0, PDM_CHAN_LEFT) |
                dmic_build_channel_map(1, 0, PDM_CHAN_RIGHT);
            break;
        }
        default: {
            sensor_data.state = AUDIO_SENSOR_STATE_ERR;
            return -EINVAL;
        }
    }

    switch (switch_pos) {
        case PRIVACY_SWITCH_POS_LOW: {
            stream.pcm_rate = LOW_SAMPLE_RATE;
            stream.block_size = MAX_BLOCK_SIZE;  // BLOCK_SIZE(LOW_SAMPLE_RATE, mode);
        } break;
        case PRIVACY_SWITCH_POS_HIGH: {
            stream.pcm_rate = HIGH_SAMPLE_RATE;
            stream.block_size = MAX_BLOCK_SIZE;  // BLOCK_SIZE(HIGH_SAMPLE_RATE, 1);

        } break;
        case PRIVACY_SWITCH_POS_OFF:
        default: {
            sensor_data.state = AUDIO_SENSOR_STATE_DISABLED;
            return -EACCES;
        }
    }
    sensor_data.audio_config.streams = &stream;
    ret = dmic_configure(dmic_dev, &sensor_data.audio_config);
    if (ret < 0) {
        LOG_ERR("DMIC configuration failed");
        return ret;
    }

    sensor_data.state = AUDIO_SENSOR_STATE_ACTIVE;

    // clean previous thread data.
    memset(&audio_sampling_thread_data, 0, sizeof(audio_sampling_thread_data));
    sensor_data.sample_iter = sample_iter;
    k_thread_create(&audio_sampling_thread_data, audio_sampling_thread_stack,
                    K_THREAD_STACK_SIZEOF(audio_sampling_thread_stack), audio_sampling_thread, NULL,
                    NULL, NULL, 6, 0, K_NO_WAIT);
    return ret;
}

int audio_sensor_stop() {
    sensor_data.state = AUDIO_SENSOR_STATE_STOP;
    //k_thread_join(&audio_sampling_thread_data, K_FOREVER);
    k_msleep(300);
    return 0;
}

int audio_sensor_init(void) {
    if (!device_is_ready(dmic_dev)) {
        LOG_ERR("%s is not ready", dmic_dev->name);
        sensor_data.state = AUDIO_SENSOR_STATE_ERR;
        return -1;
    }
    sensor_data.state = AUDIO_SENSOR_STATE_STOP;
    LOG_INF("init ok");
    return 0;
}
