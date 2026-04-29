#include "../include/LRZ.h"

LZR::LZR(int degree, std::vector<int> taps) : degree_(degree) , taps_(std::move(taps)), state_(1) , mask_((1ULL << degree) - 1) {}

void LZR::seed(uint64_t initial_state) noexcept {
    state_ = initial_state & mask_;
}


int LZR::clock() noexcept {

    int output = static_cast<int>(state_ & 1);
    int feedback = 0;

    for (int tap : taps_)
        feedback ^= static_cast<int>((state_ >> tap) & 1);

    state_ = ((state_ >> 1) | (static_cast<uint64_t>(feedback) << (degree_ - 1))) & mask_;

    return output;

}

std::vector<int> LZR::clock(int count) {

    std::vector<int> output;
    output.reserve(static_cast<std::size_t>(count));

    for (int i = 0; i < count; ++i) output.push_back(clock());

    return output;

}