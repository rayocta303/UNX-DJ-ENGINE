#pragma once

#include "raylib.h"
#include <stdbool.h>

// Component is the base interface every screen or panel must implement.
// In C, we mimic this using a struct containing function pointers and a context.

typedef struct Component Component;

struct Component {
    void *ctx;
    // Returns 0 on success, non-zero on error.
    int (*Update)(Component *self);
    // Render the component
    void (*Draw)(Component *self);
    // Destroy component
    void (*Destroy)(Component *self);
};
