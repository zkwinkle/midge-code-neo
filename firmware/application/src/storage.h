#ifndef MBFW_STORAGE_H
#define MBFW_STORAGE_H

#include <inttypes.h>
#include <stddef.h>

#define DISK_NAME "SD"
#define DISK_MOUNT_POINT "/" DISK_NAME ":"
// static const char* disk_pdrv = DISK_NAME;              // for IOCTL operations
#define MAX_PATH_LEN (MAX_FILE_NAME * 3)  // 3 levels of depth, i.e. SD/folder/file

enum mb_storage_status {
    MB_STORAGE_STATUS_UNINIT = 0,
    MB_STORAGE_STATUS_INIT_OK_INACTIVE = 1,
    MB_STORAGE_STATUS_INIT_OK_ACTIVE = 2,
    MB_STORAGE_STATUS_INIT_ERR = 3,
};

enum mb_file_type {
    FILE_TYPE_PROXIMITY = 0,
    FILE_TYPE_AUDIO = 1,
    FILE_TYPE_TIMESYNC = 2,
    FILE_TYPE_ACCEL = 3,
    FILE_TYPE_GYRO = 4,
    FILE_TYPE_MAGNETO = 5,

    // update prj.conf if this is changed to increase CONFIG_FS_FATFS_NUM_FILES
    FILE_TYPE_MAX = 5,
};


int storage_init_fs();

int storage_deinit_fs();

uint8_t storage_get_status();

int storage_init_experiment(int id);

/**
 * @brief
 *
 * @param path path to a single folder or file. In case the path matches the
 * base dir for the SD card (mount point name) then this will perform a deletion
 * of up to 2 levels of depth, i.e. SD/folder/file. Subfolders at depth level 2
 * i.e. SD/folder/subfolder will only be deleted if empty. This is done to
 * avoid recursion which is undesirable in embedded systems
 * @return int
 */
int storage_erase(char* path);

int storage_init_sample_file(enum mb_file_type file_type, int sample_iter);

uint16_t storage_get_active_sensor_bitflags();

int storage_write(enum mb_file_type file_type, void* data, size_t size);

int storage_close(enum mb_file_type file_type);

#endif
