#ifndef MB_UTILITY_H
#define MB_UTILITY_H

#include <zephyr/kernel.h>

struct SimpleWorkCtx {
    struct k_work work;
    struct k_sem done;
    int ret;
};


#endif
