#pragma once
#include "engine/shim.h"
#include <functional>

template<typename T>
inline size_t qHash(const T& t, size_t seed = 0) {
    return std::hash<T>{}(t) ^ (seed + 0x9e3779b9 + (seed << 6) + (seed >> 2));
}
