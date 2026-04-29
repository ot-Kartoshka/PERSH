#pragma once
#include <cstdint>
#include <vector>

class LZR {
public:
    LZR(int degree, std::vector<int> taps);

    void seed(uint64_t initial_state) noexcept;
    uint64_t state()  const noexcept { return state_; }
    int degree() const noexcept { return degree_; }

    int clock() noexcept;
    std::vector<int> clock(int count);

private:
    int degree_;
    std::vector<int> taps_;
    uint64_t state_ = 1;
    uint64_t mask_ = 0;
};