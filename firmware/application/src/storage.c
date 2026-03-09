#include "storage.h"

#include <errno.h>
#include <ff.h>
#include <stdio.h>
#include <zephyr/device.h>
#include <zephyr/fs/fs.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/storage/disk_access.h>

#include "status_led.h"

LOG_MODULE_REGISTER(mb_storage);

static FATFS fat_fs;
/* mounting info */
static struct fs_mount_t mp = {.type = FS_FATFS, .fs_data = &fat_fs, .mnt_point = DISK_MOUNT_POINT};

enum mb_file_status {
    MB_FILE_STATUS_INACTIVE = 0,
    MB_FILE_STATUS_ACTIVE = 1,
    MB_FILE_STATUS_ERR = 2
};

struct file_info {
    int type;                    // identifier of the file
    enum mb_file_status status;  // file status
    int ret_last;                // last operation's return value
    struct fs_file_t file;
};

struct file_info file_info_table[] = {
    {.type = FILE_TYPE_PROXIMITY, .status = MB_FILE_STATUS_INACTIVE, .ret_last = 0, .file = {}},
    {.type = FILE_TYPE_AUDIO, .status = MB_FILE_STATUS_INACTIVE, .ret_last = 0, .file = {}},
    {.type = FILE_TYPE_TIMESYNC, .status = MB_FILE_STATUS_INACTIVE, .ret_last = 0, .file = {}},
    //{.type = FILE_TYPE_ACCEL, .status = MB_FILE_STATUS_INACTIVE, .ret_last = 0, .file = {}},
    //{.type = FILE_TYPE_GYRO, .status = MB_FILE_STATUS_INACTIVE, .ret_last = 0, .file = {}},
    //{.type = FILE_TYPE_MAGNETO, .status = MB_FILE_STATUS_INACTIVE, .ret_last = 0, .file = {}},
};

#define FILE_COUNT (sizeof(file_info_table) / (sizeof(struct file_info)))
K_MUTEX_DEFINE(storage_mutex);

enum mb_storage_status storage_status = MB_STORAGE_STATUS_UNINIT;

static void storage_update_status() {
    if (k_mutex_lock(&storage_mutex, K_FOREVER) == 0) {
        switch (storage_status) {
            case MB_STORAGE_STATUS_INIT_OK_ACTIVE:
            case MB_STORAGE_STATUS_INIT_OK_INACTIVE: {
                bool active = false;
                for (int i = 0; i < FILE_COUNT; i++) {
                    if (file_info_table[i].status != MB_FILE_STATUS_INACTIVE) {
                        active = true;
                        break;
                    }
                }
                storage_status =
                    active ? MB_STORAGE_STATUS_INIT_OK_ACTIVE : MB_STORAGE_STATUS_INIT_OK_INACTIVE;
                led_report_active(active);
            } break;
            default: {
            }
        }
        k_mutex_unlock(&storage_mutex);
    }
}

K_THREAD_STACK_DEFINE(storage_sync_thread_stack, 1024);
struct k_thread storage_sync_thread_data;
void storage_sync_thread(void* p1, void* p2, void* p3) {
    while (true) {
        k_msleep(100);
        if (k_mutex_lock(&storage_mutex, K_MSEC(20)) != 0) {
            LOG_ERR("could not acquire storage mutex, sync thread");
            continue;
        }

        if ((storage_status != MB_STORAGE_STATUS_INIT_OK_ACTIVE) &&
            (storage_status != MB_STORAGE_STATUS_INIT_OK_INACTIVE)) {
            LOG_INF("exiting sync thread");
            k_mutex_unlock(&storage_mutex);
            return;
        }
        for (int i = 0; i < FILE_COUNT; i++) {
            if (file_info_table[i].status == MB_FILE_STATUS_ACTIVE) {
                int ret = fs_sync(&file_info_table[i].file);
                if (ret < 0) {
                    file_info_table[i].status = MB_FILE_STATUS_ERR;
                    file_info_table[i].ret_last = ret;
                }
            }
        }

        k_mutex_unlock(&storage_mutex);
    }
}

int storage_init_fs() {
    if (k_mutex_lock(&storage_mutex, K_FOREVER) != 0) {
        LOG_ERR("could not acquire storage mutex to init fs");
        return -EACCES;
    }

    int res = 0;
    if ((storage_status != MB_STORAGE_STATUS_INIT_OK_ACTIVE) &&
        (storage_status != MB_STORAGE_STATUS_INIT_OK_INACTIVE)) {
        res = fs_mount(&mp);
        if (res == FR_OK) {
            storage_status = MB_STORAGE_STATUS_INIT_OK_INACTIVE;
            LOG_INF("Disk mounted");
        } else {
            storage_status = MB_STORAGE_STATUS_INIT_ERR;
            LOG_ERR("Error mounting disk.");
        }
    } else {
        LOG_INF("storage already initialized");
    }
    k_mutex_unlock(&storage_mutex);

    memset(&storage_sync_thread_data, 0, sizeof(struct k_thread));
    k_thread_create(&storage_sync_thread_data, storage_sync_thread_stack,
                    K_THREAD_STACK_SIZEOF(storage_sync_thread_stack), storage_sync_thread, NULL,
                    NULL, NULL, 5, 0, K_NO_WAIT);

    if (res == FR_OK) {
        storage_init_experiment(0);
    }
    return res;
}

int storage_deinit_fs() {
    int res = 0;

    if (storage_status == MB_STORAGE_STATUS_INIT_OK_ACTIVE) {
        LOG_ERR("Cannot deinit fs while sampling is ongoing");
        res = -EACCES;
    } else {
        storage_status = MB_STORAGE_STATUS_UNINIT;
        if (k_mutex_lock(&storage_mutex, K_FOREVER) != 0) {
            LOG_ERR("could not acquire storage mutex to deinit fs");
            return -EACCES;
        }

        res = fs_unmount(&mp);
        LOG_INF("Disk unmounted 1");
        k_mutex_unlock(&storage_mutex);
        k_thread_join(&storage_sync_thread_data, K_FOREVER);
        LOG_INF("Disk unmounted 2");
    }
    return res;
}

uint8_t storage_get_status() { return (uint8_t)storage_status; }

#define MAX_PATH_SIZE 128
// should be protected via mutex
char current_dir[MAX_PATH_SIZE];

int storage_init_experiment(int id) {
    if (k_mutex_lock(&storage_mutex, K_FOREVER) != 0) {
        LOG_ERR("could not acquire storage mutex to init experiment");
        return -EACCES;
    }
    int ret = 0;
    if (storage_status != MB_STORAGE_STATUS_INIT_OK_INACTIVE) {
        LOG_ERR("Cannot init experiment folder while sampling is ongoing");
        ret = -EACCES;
    } else if (snprintf(current_dir, MAX_PATH_SIZE, "/" DISK_NAME ":/%d", id) > MAX_PATH_SIZE) {
        ret = -ENAMETOOLONG;
    } else {
        LOG_INF("creating experiment folder %s", current_dir);
        ret = fs_mkdir(current_dir);
        if ((ret != 0) && (ret != -EEXIST)) {
            LOG_ERR("Unknown error trying to create folder");
        }
        LOG_INF("Experiment folder created: %d", id);
    }
    k_mutex_unlock(&storage_mutex);
    return ret;
}

int storage_erase(char* path) {
    int res;
    if (strcmp(path, mp.mnt_point) == 0) {
        LOG_INF("erasing %s", path);
        res = storage_deinit_fs();
        if (res < 0) {
            LOG_ERR("could not unmount , errno: %d", res);
            return res;
        }

        // check files are closed
        if (k_mutex_lock(&storage_mutex, K_FOREVER) != 0) {
            LOG_ERR("could not acquire storage mutex to erase");
            return -EACCES;
        }

        if (storage_status != MB_STORAGE_STATUS_UNINIT) {
            res = -EACCES;
        } else {
            res = fs_mkfs(FS_FATFS, (uintptr_t)DISK_NAME ":", NULL, 0);
            if (res < 0) {
                LOG_ERR("Error formating persistent storage %d", res);
            }else{}
        }
        k_mutex_unlock(&storage_mutex);
        if (res == 0) {
            res = storage_init_fs();
            if (res < 0) {
                LOG_ERR("failed to init fs after formatting");
            }
        }
    } else {
        // other patch provided
        LOG_ERR("Erasing single files not yet supported");
        res = -EINVAL;
        /*printf("about to delete %s\n", path);
        res = fs_unlink(path);
        if (res) {
            printf("problema eliminando %d\n", res);
        }*/
    }

    return res;
}

// assumes experiment was already initialized
int storage_init_sample_file(enum mb_file_type file_type, int sample_iter) {
    if ((storage_status == MB_STORAGE_STATUS_UNINIT) ||
        (storage_status == MB_STORAGE_STATUS_INIT_ERR)) {
        LOG_ERR("cannot init sample file! fs not initialized!");
        return -EPERM;
    }
    if (file_type > FILE_TYPE_MAX) {
        return -EINVAL;
    }

    if (file_info_table[file_type].status == MB_FILE_STATUS_ACTIVE) {
        return -EINPROGRESS;
    }

    if (file_info_table[file_type].status == MB_FILE_STATUS_ERR) {
        LOG_ERR("file of type %d init after err", file_type);
    }

    int ret;
    memset(&file_info_table[file_type].file, 0, sizeof(struct fs_file_t));
    fs_file_t_init(&file_info_table[file_type].file);
    const char* fmt_prox = "%s/PROX%d";
    const char* fmt_sync = "%s/SYNC%d";
    const char* fmt_mic = "%s/MIC%d";
    const char* fmt_accel = "%s/ACC%d";
    const char* fmt_gyro = "%s/GYR%d";
    const char* fmt_magneto = "%s/MAG%d";

    const char* dummy = "DUMMY";
    const char* fmt = dummy;
    switch (file_type) {
        case (FILE_TYPE_PROXIMITY): {
            fmt = fmt_prox;
        } break;
        case (FILE_TYPE_TIMESYNC): {
            fmt = fmt_sync;
        } break;
        case (FILE_TYPE_AUDIO): {
            fmt = fmt_mic;
        } break;
        case (FILE_TYPE_ACCEL): {
            fmt = fmt_accel;
        } break;
        case (FILE_TYPE_GYRO): {
            fmt = fmt_gyro;
        } break;
        case (FILE_TYPE_MAGNETO): {
            fmt = fmt_magneto;
        } break;
        default: {
            return -EINVAL;
        }
    }
    char path[MAX_PATH_SIZE];
    snprintf(path, MAX_PATH_SIZE, fmt, current_dir, sample_iter);
    LOG_INF("Trying to open file: %s", path);
    ret = fs_open(&file_info_table[file_type].file, path, FS_O_CREATE | FS_O_WRITE);
    file_info_table[file_type].status = (ret < 0) ? MB_FILE_STATUS_ERR : MB_FILE_STATUS_ACTIVE;
    file_info_table[file_type].ret_last = ret;
    storage_update_status();
    return ret;
}

uint16_t storage_get_active_sensor_bitflags() {
    uint16_t bitflags = 0;
    for (int i = 0; i < FILE_COUNT; i++) {
        if (file_info_table[i].status == MB_FILE_STATUS_ACTIVE) {
            bitflags |= (1 << file_info_table[i].type);
        }
    }
    return bitflags;
}

int storage_write(enum mb_file_type file_type, void* data, size_t size) {
    if (file_info_table[file_type].status == MB_FILE_STATUS_ERR) {
        LOG_ERR("write to file_type %d with err status %d", file_type,
                file_info_table[file_type].ret_last);
    }
    if (file_info_table[file_type].status == MB_FILE_STATUS_INACTIVE) {
        LOG_ERR("write to unopened file_type %d", file_type);
    }
    int ret = fs_write(&file_info_table[file_type].file, data, size);
#ifdef STORAGE_DMA_NOT_ENABLED
    fs_sync(&file_info_table[file_type].file);
#endif
    file_info_table[file_type].status = (ret < 0) ? MB_FILE_STATUS_ERR : MB_FILE_STATUS_ACTIVE;
    file_info_table[file_type].ret_last = ret;
    return ret;
}

int storage_close(enum mb_file_type file_type) {
    if (k_mutex_lock(&storage_mutex, K_FOREVER) != 0) {
        LOG_ERR("could not acquire storage mutex to close file");
        return -EACCES;
    }
    int ret = fs_close(&file_info_table[file_type].file);
    file_info_table[file_type].status = (ret < 0) ? MB_FILE_STATUS_ERR : MB_FILE_STATUS_INACTIVE;
    file_info_table[file_type].ret_last = ret;
    k_mutex_unlock(&storage_mutex);
    storage_update_status();
    return ret;
}
