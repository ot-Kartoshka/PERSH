#pragma once
#include <cstdint>
#include <span>
#include <string>
#include <vector>
#include "LRZ.h"
#include "MathUtils.h"


std::vector<uint64_t> find_candidates( const LZR& lfsr_template, std::span<const int> keystream, const CorrelationParams& params);

std::vector<uint64_t> find_l3_candidates( const LZR& l3_template, std::span<const int> x_seq, std::span<const int> y_seq, std::span<const int> keystream);

bool verify_key( LZR l1, uint64_t s1, LZR l2, uint64_t s2, LZR l3, uint64_t s3, std::span<const int> keystream);

std::string to_binary(uint64_t state, int degree);