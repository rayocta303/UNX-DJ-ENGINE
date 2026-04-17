#pragma once
#include "engine/shim.h"

class PollingControlProxy {
public:
    PollingControlProxy() {}
    double get() const { return 0.0; }
    void set(double) {}
};
