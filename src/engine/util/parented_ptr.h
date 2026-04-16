#pragma once
#include <memory>

template<typename T>
using parented_ptr = std::unique_ptr<T>;
