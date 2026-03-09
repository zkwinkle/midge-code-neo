#ifndef MB_AUDIO_H
#define MB_AUDIO_H

#include <inttypes.h>

uint8_t audio_sensor_get_status();

int audio_sensor_init();

enum audio_mode{
    AUDIO_MODE_MONO = 1,
    AUDIO_MODE_STEREO = 2 ,
};

//int proximity_sensor_change_config(uint16_t interval, uint16_t window);

int audio_sensor_start(int sample_iter, int mode);

int audio_sensor_stop();


#endif
