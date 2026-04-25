#ifndef HID_MAPPER_H
#define HID_MAPPER_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint16_t vendorId;
    uint16_t productId;
    char name[128];
    char scriptFiles[8][128];
    int scriptCount;
} HidMapping;

bool HID_LoadMapping(HidMapping *map, const char *path);

#endif
