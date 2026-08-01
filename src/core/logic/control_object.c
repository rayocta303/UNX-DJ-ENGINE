#include "core/logic/control_object.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define MAX_CONTROLS 1024
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
            printf("[CO SET] Group: '%s' Key: '%s' -> Value: %.3f\n", group, key, realVal);
            return;
        }
    }
    printf("[CO MISS] Unregistered CO Group: '%s' Key: '%s'\n", group, key);
}

void CO_AddValue(const char *group, const char *key, float delta) {
    for (int i = 0; i < controlCount; i++) {
        if (strcmp(registry[i].group, group) == 0 && strcmp(registry[i].key, key) == 0) {
            ControlObject *co = &registry[i];
            if (co->type == CO_TYPE_FLOAT) {
                *((float*)co->ptr) += delta;
            } else if (co->type == CO_TYPE_INT) {
                *((int*)co->ptr) += (int)delta;
            }
            return;
        }
    }
}

float CO_GetValue(const char *group, const char *key) {
    for (int i = 0; i < controlCount; i++) {
        if (strcmp(registry[i].group, group) == 0 && strcmp(registry[i].key, key) == 0) {
            ControlObject *co = &registry[i];
            if (co->type == CO_TYPE_FLOAT) {
                return *((float*)co->ptr);
            } else if (co->type == CO_TYPE_BOOL) {
                return *((bool*)co->ptr) ? 1.0f : 0.0f;
            } else if (co->type == CO_TYPE_INT) {
                return (float)(*((int*)co->ptr));
            }
        }
    }
    return 0.0f;
}

float CO_GetValueNormalized(const char *group, const char *key) {
    for (int i = 0; i < controlCount; i++) {
        if (strcmp(registry[i].group, group) == 0 && strcmp(registry[i].key, key) == 0) {
            ControlObject *co = &registry[i];
            float val = CO_GetValue(group, key);
            float range = co->max - co->min;
            if (range <= 0.0001f) return 0.0f;
            float norm = (val - co->min) / range;
            if (norm < 0.0f) norm = 0.0f;
            if (norm > 1.0f) norm = 1.0f;
            return norm;
        }
    }
    return 0.0f;
}

void CO_ToggleValue(const char *group, const char *key) {
    for (int i = 0; i < controlCount; i++) {
        if (strcmp(registry[i].group, group) == 0 && strcmp(registry[i].key, key) == 0) {
            ControlObject *co = &registry[i];
            if (co->type == CO_TYPE_BOOL) {
                bool *p = (bool*)co->ptr;
                *p = !(*p);
            } else if (co->type == CO_TYPE_FLOAT) {
                float *p = (float*)co->ptr;
                *p = (*p > 0.5f) ? 0.0f : 1.0f;
            } else if (co->type == CO_TYPE_INT) {
                int *p = (int*)co->ptr;
                *p = (*p > 0) ? 0 : 1;
            }
            return;
        }
    }
}

int CO_GetCount() {
    return controlCount;
}

ControlObject* CO_GetByIndex(int idx) {
    if (idx < 0 || idx >= controlCount) return NULL;
    return &registry[idx];
}
