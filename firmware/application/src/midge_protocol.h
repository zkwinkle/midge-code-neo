#ifndef MB_PROTOCOL_H
#define MB_PROTOCOL_H

#include <inttypes.h>


// ==== Miscellaneous data types ===== //
union __attribute__((packed)) BadgeAssignment {
    struct __attribute__((packed)) {
        uint16_t group : 4;
        uint16_t badge : 12;
    } badge_id; // little endian assumed
    uint16_t u16_all;
};

struct __attribute__((packed)) CustomAdvertisementData {
    uint16_t battery_mv;
    uint16_t active_sensor_bitflags;
    union BadgeAssignment badge_assignment;
};

// ==== Protocol messages =========== //
struct __attribute__((packed)) CmdSetupExperimentRequest{
  union BadgeAssignment badge_assignment;
  uint16_t experiment_id;
};

struct __attribute__((packed)) CmdSetupExperimentResponse{
  uint8_t status_code;
};

// uint16_t configured_datarate

struct __attribute__((packed)) CmdStatusRequest{
  uint64_t  millis_since_epoch;
};
struct __attribute__((packed)) CmdStatusResponse{
  uint8_t sync_status;
  uint8_t storage_init_status;
  uint8_t audio_init_status;
  uint8_t proximity_init_status;
  int16_t battery_millivolts;
  union BadgeAssignment badge_assignment;
  uint64_t sync_delta_ms;
};

struct __attribute__((packed)) CmdGetFWVersionRequest{
  uint16_t  reserved;
};

struct __attribute__((packed)) CmdGetFWVersionResponse{
  uint8_t version_str[32];
};

struct __attribute__((packed)) CmdStartMicRequest{
  uint16_t sample_id;
  uint8_t mode; // See @ref audio.h for mode definitions
};

struct __attribute__((packed)) CmdStartMicResponse{
  int32_t status_code;
};

struct __attribute__((packed)) CmdStopMicRequest{
  uint16_t  reserved;
};

struct __attribute__((packed)) CmdStopMicResponse{
  int32_t status_code;
};

struct __attribute__((packed)) CmdStartScanRequest{
  uint16_t sample_id;
  uint16_t window;
  uint16_t interval;
  uint16_t reserved;
};

struct __attribute__((packed)) CmdStartScanResponse{
  int32_t status_code;
};

struct __attribute__((packed)) CmdStopScanRequest{
  uint16_t  reserved;
};

struct __attribute__((packed)) CmdStopScanResponse{
  int32_t status_code;
};

struct __attribute__((packed)) CmdEraseSDRequest{
  uint16_t  reserved;
};

struct __attribute__((packed)) CmdEraseSDResponse{
  int32_t status_code;
};

#endif
