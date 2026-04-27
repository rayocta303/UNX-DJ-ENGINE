#if defined(_WIN32) || defined(__CYGWIN__)
#include "core/hid/hid_backend.h"
#include <windows.h>
#include <setupapi.h>
#include <hidsdi.h>
#include <stdio.h>

#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "hid.lib")

bool HID_Init(void) {
    return true;
}

int HID_Scan(HidDevice *outDevices, int maxDevices) {
    GUID hidGuid;
    HidD_GetHidGuid(&hidGuid);
    HDEVINFO deviceInfoSet = SetupDiGetClassDevsA(&hidGuid, NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    
    if (deviceInfoSet == INVALID_HANDLE_VALUE) return 0;

    SP_DEVICE_INTERFACE_DATA deviceInterfaceData;
    deviceInterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);
    
    int count = 0;
    for (DWORD i = 0; SetupDiEnumDeviceInterfaces(deviceInfoSet, NULL, &hidGuid, i, &deviceInterfaceData) && count < maxDevices; i++) {
        DWORD detailSize = 0;
        SetupDiGetDeviceInterfaceDetailA(deviceInfoSet, &deviceInterfaceData, NULL, 0, &detailSize, NULL);
        
        PSP_DEVICE_INTERFACE_DETAIL_DATA_A detailData = (PSP_DEVICE_INTERFACE_DETAIL_DATA_A)malloc(detailSize);
        detailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_A);
        
        if (SetupDiGetDeviceInterfaceDetailA(deviceInfoSet, &deviceInterfaceData, detailData, detailSize, NULL, NULL)) {
            HANDLE h = CreateFileA(detailData->DevicePath, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
            if (h != INVALID_HANDLE_VALUE) {
                HIDD_ATTRIBUTES attr;
                attr.Size = sizeof(HIDD_ATTRIBUTES);
                if (HidD_GetAttributes(h, &attr)) {
                    outDevices[count].vendorId = attr.VendorID;
                    outDevices[count].productId = attr.ProductID;
                    strncpy(outDevices[count].name, "HID Device", 127);
                    
                    WCHAR productStr[128];
                    if (HidD_GetProductString(h, productStr, sizeof(productStr))) {
                        wcstombs(outDevices[count].name, productStr, 127);
                    }
                    
                    CloseHandle(h);
                    count++;
                } else {
                    CloseHandle(h);
                }
            }
        }
        free(detailData);
    }

    SetupDiDestroyDeviceInfoList(deviceInfoSet);
    return count;
}

bool HID_Open(HidDevice *dev) {
    // Similar to Scan, we'd find the DevicePath again or store it
    // For now, let's assume we store the path in a hidden field
    return false; // Stub
}

void HID_Close(HidDevice *dev) {
    if (dev->handle) {
        CloseHandle((HANDLE)dev->handle);
        dev->handle = NULL;
    }
}

int HID_Read(HidDevice *dev, uint8_t *buffer, int length) {
    if (!dev->handle) return -1;
    DWORD read = 0;
    if (ReadFile((HANDLE)dev->handle, buffer, length, &read, NULL)) {
        return (int)read;
    }
    return -1;
}

int HID_Write(HidDevice *dev, const uint8_t *buffer, int length) {
    if (!dev->handle) return -1;
    DWORD written = 0;
    if (WriteFile((HANDLE)dev->handle, buffer, length, &written, NULL)) {
        return (int)written;
    }
    return -1;
}
#else
// Stub implementations for non-Windows platforms to satisfy the linker
#include "core/hid/hid_backend.h"

bool HID_Init(void) { return true; }
int HID_Scan(HidDevice *outDevices, int maxDevices) { (void)outDevices; (void)maxDevices; return 0; }
bool HID_Open(HidDevice *dev) { (void)dev; return false; }
void HID_Close(HidDevice *dev) { (void)dev; }
int HID_Read(HidDevice *dev, uint8_t *buffer, int length) { (void)dev; (void)buffer; (void)length; return -1; }
int HID_Write(HidDevice *dev, const uint8_t *buffer, int length) { (void)dev; (void)buffer; (void)length; return -1; }
#endif
