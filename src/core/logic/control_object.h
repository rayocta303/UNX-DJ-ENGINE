#ifndef CONTROL_OBJECT_H
#define CONTROL_OBJECT_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    CO_TYPE_FLOAT,
    CO_TYPE_BOOL,
    CO_TYPE_INT
} COType;

typedef struct {
    char group[32]; // e.g., "[Channel1]"
    char key[64];   // e.g., "volume"
    COType type;
    void *ptr;
    float min;
    float max;
} ControlObject;

void CO_Init();
void CO_Register(const char *group, const char *key, COType type, void *ptr, float min, float max);
void* CO_Find(const char *group, const char *key, COType *outType);
void CO_SetValue(const char *group, const char *key, float normalizedValue);
void CO_AddValue(const char *group, const char *key, float delta);
int CO_GetCount();
ControlObject* CO_GetByIndex(int idx);

#endif
