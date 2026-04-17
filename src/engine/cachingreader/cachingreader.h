#pragma once
#include <vector>
#include "engine/shim.h"

class HintVector : public std::vector<int> {};
class CachingReader {
public:
    virtual ~CachingReader() = default;
};
