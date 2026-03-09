#ifndef MB_SWITCH_H
#define MB_SWITCH_H

enum privacy_sw_pos {
    PRIVACY_SWITCH_POS_OFF,
    PRIVACY_SWITCH_POS_LOW,
    PRIVACY_SWITCH_POS_HIGH,
};

int switch_sensor_init();

enum privacy_sw_pos switch_sensor_position();

#endif
