#ifndef HID_BACKEND_H
#define HID_BACKEND_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint16_t vendorId;
    uint16_t productId;
    char name[128];
    void *handle;
} HidDevice;

bool HID_Init(void);
int HID_Scan(HidDevice *outDevices, int maxDevices);
bool HID_Open(HidDevice *dev);
void HID_Close(HidDevice *dev);
int HID_Read(HidDevice *dev, uint8_t *buffer, int length);
int HID_Write(HidDevice *dev, const uint8_t *buffer, int length);

#endif
