#include "core/logic/control_object.h"
#include <string.h>
#include <stdlib.h>

#define MAX_CONTROLS 256
static ControlObject registry[MAX_CONTROLS];
static int controlCount = 0;

void CO_Init() {
    controlCount = 0;
}

void CO_Register(const char *group, const char *key, COType type, void *ptr, float min, float max) {
    if (controlCount >= MAX_CONTROLS) return;
    
    ControlObject *co = &registry[controlCount++];
    strncpy(co->group, group, 31);
    strncpy(co->key, key, 63);
    co->type = type;
    co->ptr = ptr;
    co->min = min;
    co->max = max;
}

void* CO_Find(const char *group, const char *key, COType *outType) {
    for (int i = 0; i < controlCount; i++) {
        if (strcmp(registry[i].group, group) == 0 && strcmp(registry[i].key, key) == 0) {
            if (outType) *outType = registry[i].type;
            return registry[i].ptr;
        }
    }
    return NULL;
}

void CO_SetValue(const char *group, const char *key, float normalizedValue) {
    for (int i = 0; i < controlCount; i++) {
        if (strcmp(registry[i].group, group) == 0 && strcmp(registry[i].key, key) == 0) {
            ControlObject *co = &registry[i];
            float realVal = co->min + normalizedValue * (co->max - co->min);
            
            if (co->type == CO_TYPE_FLOAT) {
                *((float*)co->ptr) = realVal;
            } else if (co->type == CO_TYPE_BOOL) {
                *((bool*)co->ptr) = (normalizedValue > 0.5f);
            } else if (co->type == CO_TYPE_INT) {
                *((int*)co->ptr) = (int)realVal;
            }
            return;
        }
    }
}
