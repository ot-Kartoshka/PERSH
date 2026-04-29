#include "../include/Geffe.h"

GeffeGenerator::GeffeGenerator(LZR l1, LZR l2, LZR l3) : l1_(std::move(l1)), l2_(std::move(l2)), l3_(std::move(l3)) {}

void GeffeGenerator::seed(uint64_t s1, uint64_t s2, uint64_t s3) {
    l1_.seed(s1);
    l2_.seed(s2);
    l3_.seed(s3);
}

int GeffeGenerator::clock() noexcept {

    int x = l1_.clock();
    int y = l2_.clock();
    int s = l3_.clock();
    return (s & x) ^ ((s ^ 1) & y);

}

std::vector<int> GeffeGenerator::clock(int count) {

    std::vector<int> output;
    output.reserve(static_cast<std::size_t>(count));

    for (int i = 0; i < count; ++i) output.push_back(clock());

    return output;

}