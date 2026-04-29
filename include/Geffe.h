#pragma once
#include <cstdint>
#include <vector>
#include "LRZ.h"

class GeffeGenerator {
public:
    GeffeGenerator(LZR l1, LZR l2, LZR l3);

    void seed(uint64_t s1, uint64_t s2, uint64_t s3);

    int clock() noexcept;
    std::vector<int> clock(int count);

private:
    LZR l1_, l2_, l3_;
};