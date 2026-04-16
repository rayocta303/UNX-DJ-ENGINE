#pragma once
#include <span>

namespace unx {
namespace spanutil {
    template<typename T>
    std::span<T> spanFromPtrLen(T* data, size_t len) {
        return std::span<T>(data, len);
    }
}
}
