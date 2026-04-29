#include "../include/Attack.h"
#include <iostream>
#include <print>
#include <thread>
#include <mutex>
#include <cmath>

static std::vector<uint64_t> sweep(const LZR& LZR_template, std::span<const int> z, int N, int C_int) {

    int n = LZR_template.degree();
    uint64_t mask = (1ULL << n) - 1;

    std::vector<uint64_t> candidates;
    std::mutex mtx;

    unsigned int hw_cores = std::thread::hardware_concurrency();
    unsigned int num_threads = (hw_cores > 1) ? (hw_cores / 2) : 1;

    uint64_t chunk_size = mask / num_threads;

    auto worker = [&](uint64_t start, uint64_t end) {
        LZR local_LZR = LZR_template;
        std::vector<uint64_t> local_cands;

        for (uint64_t init = start; init <= end; ++init) {
            if (init == 0) continue;

            local_LZR.seed(init);
            int R = 0;
            bool is_valid = true;

            for (int i = 0; i < N; ++i) {
                if ((local_LZR.clock() ^ z[i]) == 1) {
                    R++;
                }
                if (R > C_int) {
                    is_valid = false;
                    break;
                }
            }

            if (is_valid) {
                local_cands.push_back(init);
            }
        }

        mtx.lock();
        for (uint64_t cand : local_cands) {
            candidates.push_back(cand);
        }
        mtx.unlock();
        };

    std::vector<std::thread> threads;

    for (unsigned int i = 0; i < num_threads; ++i) {
        uint64_t start = i * chunk_size;
        if (i == 0) start = 1; 

        uint64_t end = start + chunk_size - 1;
        if (i == num_threads - 1) end = mask; 

        threads.push_back(std::thread(worker, start, end));
    }

    for (auto& t : threads) {
        t.join();
    }


    return candidates;
}

std::vector<uint64_t> find_candidates( const LZR& LZR_template, std::span<const int> keystream, const CorrelationParams& params) {
    auto candidates = sweep(LZR_template, keystream, params.sequence_length, static_cast<int>(params.threshold));

    if (!candidates.empty())
        return candidates;

    int C_wide = static_cast<int>(std::ceil(params.sequence_length * 0.375));
    std::println("  [попередження] Строгий поріг C={:.2f} не дав кандидатів.", params.threshold);
    std::println("  Другий прохід із розширеним порогом C={}...", C_wide);

    return sweep(LZR_template, keystream, params.sequence_length, C_wide);
}

std::vector<uint64_t> find_l3_candidates( const LZR& l3_template, std::span<const int> x_seq, std::span<const int> y_seq, std::span<const int> keystream) {
    int N = static_cast<int>(std::min({ x_seq.size(), y_seq.size(), keystream.size() }));

    std::vector<int> constrained_positions, required_bits;
    for (int i = 0; i < N; ++i) {
        if (x_seq[i] != y_seq[i]) {
            constrained_positions.push_back(i);
            required_bits.push_back(keystream[i] == x_seq[i] ? 1 : 0);
        }
    }

    if (constrained_positions.empty()) return {};

    int n = l3_template.degree();
    uint64_t mask = (1ULL << n) - 1;
    int last_pos = constrained_positions.back();
    int n_constr = static_cast<int>(constrained_positions.size());

    std::vector<uint64_t> candidates;
    std::mutex mtx;

    unsigned int hw_cores = std::thread::hardware_concurrency();
    unsigned int num_threads = (hw_cores > 1) ? (hw_cores / 2) : 1;

    uint64_t chunk_size = mask / num_threads;

    auto worker = [&](uint64_t start, uint64_t end) {
        LZR l3 = l3_template;
        std::vector<uint64_t> local_cands;

        for (uint64_t init = start; init <= end; ++init) {
            if (init == 0) continue;
            l3.seed(init);
            bool ok = true;
            int  ci = 0;

            for (int i = 0; i <= last_pos && ci < n_constr; ++i) {
                int bit = l3.clock();
                if (i == constrained_positions[ci]) {
                    if (bit != required_bits[ci]) { ok = false; break; }
                    ++ci;
                }
            }

            if (ok) local_cands.push_back(init);

        }

        mtx.lock();

        for (uint64_t cand : local_cands) {
            candidates.push_back(cand);
        }
        mtx.unlock();

        };

    std::vector<std::thread> threads;

    for (unsigned int i = 0; i < num_threads; ++i) {
        uint64_t start = i * chunk_size;
        if (i == 0) start = 1;

        uint64_t end = start + chunk_size - 1;
        if (i == num_threads - 1) end = mask;

        threads.push_back(std::thread(worker, start, end));
    }

    for (auto& t : threads) {
        t.join();
    }

    return candidates;
}

bool verify_key( LZR l1, uint64_t s1, LZR l2, uint64_t s2, LZR l3, uint64_t s3, std::span<const int> keystream) {
    l1.seed(s1); l2.seed(s2); l3.seed(s3);
    for (int expected : keystream) {
        int x = l1.clock(), y = l2.clock(), s = l3.clock();
        if (((s & x) ^ ((s ^ 1) & y)) != expected) return false;
    }
    return true;
}

std::string to_binary(uint64_t state, int degree) {
    std::string result(static_cast<std::size_t>(degree), '0');
    for (int i = 0; i < degree; ++i)
        result[static_cast<std::size_t>(i)] = ((state >> i) & 1) ? '1' : '0';
    return result;
}