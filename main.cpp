#include <algorithm>
#include <charconv>
#include <chrono>
#include <cmath>
#include <expected>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <print>
#include <string>
#include <vector>

#include "include/Attack.h"
#include "include/Errors.h"
#include "include/Geffe.h"
#include "include/LRZ.h"
#include "include/MathUtils.h"

static LZR make_l1(bool simplified) {
    if (simplified) return { 25, {0, 3} };
    return { 30, {0, 1, 4, 6} };
}

static LZR make_l2(bool simplified) {
    if (simplified) return { 26, {0, 1, 2, 6} };
    return { 31, {0, 3} };
}

static LZR make_l3(bool simplified) {
    if (simplified) return { 27, {0, 1, 2, 5} };
    return { 32, {0, 1, 2, 3, 5, 7} };
}


static std::vector<int> g_keystream;
static std::vector<uint64_t> g_l1_candidates;
static std::vector<uint64_t> g_l2_candidates;
static bool g_key_found = false;
static uint64_t g_key1 = 0, g_key2 = 0, g_key3 = 0;
static bool g_simplified = false;
static double g_alpha = 0.01;

static std::expected<std::vector<int>, Error> load_sequence( const std::filesystem::path& path, int variant) {

    std::ifstream file(path);
    if (!file) return std::unexpected(Error::FileNotFound);

    std::string line;
    std::string target = "Variant " + std::to_string(variant);

    while (std::getline(file, line))
        if (line == target) break;

    if (file.eof()) return std::unexpected(Error::VariantNotFound);

    while (std::getline(file, line))
        if (!line.empty()) break;

    std::vector<int> z;
    z.reserve(line.size());
    for (char c : line)
        if (c == '0' || c == '1') z.push_back(c - '0');

    if (z.empty()) return std::unexpected(Error::VariantNotFound);
    return z;
}

static bool parse_int(std::string_view s, int& out) noexcept {
    auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), out);
    return ec == std::errc{} && ptr == s.data() + s.size();
}

static bool parse_uint64(std::string_view s, uint64_t& out) noexcept {
    auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), out);
    return ec == std::errc{} && ptr == s.data() + s.size();
}

static void reset_attack_state() {
    g_keystream.clear();
    g_l1_candidates.clear();
    g_l2_candidates.clear();
    g_key_found = false;
    g_key1 = g_key2 = g_key3 = 0;
}

static void do_mode() {

    std::print("  [1] спрощений (n=25/26/27)  [2] основний (n=30/31/32)  (Enter = без змін): ");
    std::cout.flush();
    std::string s;
    std::getline(std::cin, s);

    if (s == "1") { g_simplified = true;  reset_attack_state(); }
    if (s == "2") { g_simplified = false; reset_attack_state(); }

    std::println("  Поточний режим: {}.", g_simplified ? "спрощений" : "основний");

}

static void do_load() {

    std::print("  Шлях до файлу: ");
    std::cout.flush();
    std::string path;
    std::getline(std::cin, path);

    std::print("  Номер варіанту: ");
    std::cout.flush();
    std::string s;
    std::getline(std::cin, s);
    int variant = 0;

    if (!parse_int(s, variant) || variant < 1) {
        std::println("    {}", error_to_string(Error::InvalidArgument));
        return;
    }

    auto result = load_sequence(path, variant);
    if (!result) {
        std::println("    {}", error_to_string(result.error()));
        return;
    }

    g_keystream = std::move(*result);
    g_l1_candidates.clear();
    g_l2_candidates.clear();
    g_key_found = false;

    std::println("  Завантажено {} бітів (варіант {}).", g_keystream.size(), variant);
    std::print("  Перші 64 біти: ");
    for (int i = 0; i < std::min(64, (int)g_keystream.size()); ++i)
        std::print("{}", g_keystream[i]);

    std::println();
}

static void do_show_params() {
    auto l1 = make_l1(g_simplified);
    auto l2 = make_l2(g_simplified);
    auto l3 = make_l3(g_simplified);

    auto p1 = compute_params(l1.degree(), g_alpha);
    auto p2 = compute_params(l2.degree(), g_alpha);

    std::println("\n  Режим: {}    α = {}", g_simplified ? "спрощений (n=25/26/27)" : "основний (n=30/31/32)", g_alpha);
    std::println("\n  +------------------------------------------------------------+");
    std::println("  │ Регістр         │  N*   │    C     │  t(1-α)   │  t(1-β)   │");
    std::println("  +-----------------+-------+----------+-----------+-----------+");
    std::println("  │ L1  (n={:2d})      │ {:>5} │ {:>8.3f} │ {:>9.4f} │ {:>9.4f} │", l1.degree(), p1.sequence_length, p1.threshold, p1.t_alpha, p1.t_beta);
    std::println("  │ L2  (n={:2d})      │ {:>5} │ {:>8.3f} │ {:>9.4f} │ {:>9.4f} │", l2.degree(), p2.sequence_length, p2.threshold, p2.t_alpha, p2.t_beta);
    std::println("  +-----------------+-------+----------+-----------+-----------+");

    std::println("\n  Трудомісткість:");
    std::println("    Кореляційна атака:  2^{} + 2^{} + 2^{}  =  {:.2e}", l1.degree(), l2.degree(), l3.degree(), std::ldexp(1.0, l1.degree()) + std::ldexp(1.0, l2.degree()) + std::ldexp(1.0, l3.degree()));
    std::println("    Повний перебір:     2^{}  =  {:.2e}", l1.degree() + l2.degree() + l3.degree(), std::ldexp(1.0, l1.degree() + l2.degree() + l3.degree()));
}

static void do_geffe_test() {

    auto ask_state = [](const char* name) -> uint64_t {
        std::print("  Початковий стан {} (Enter = 1): ", name);
        std::cout.flush();
        std::string s;
        std::getline(std::cin, s);
        if (s.empty()) return 1;
        uint64_t v = 0;
        parse_uint64(s, v);
        return v ? v : 1;
        };

    uint64_t s1 = ask_state("L1");
    uint64_t s2 = ask_state("L2");
    uint64_t s3 = ask_state("L3");

    std::print("  Кількість бітів (Enter = 64): ");
    std::cout.flush();
    std::string s;
    std::getline(std::cin, s);
    int count = 64;
    if (!s.empty()) { int v = 0; if (parse_int(s, v) && v > 0) count = v; }

    GeffeGenerator gen(make_l1(g_simplified), make_l2(g_simplified), make_l3(g_simplified));
    gen.seed(s1, s2, s3);
    auto output = gen.clock(count);

    std::println("\n  Згенерована гама ({} бітів):", count);

    for (int i = 0; i < count; ++i) {
        std::print("{}", output[i]);
        if ((i + 1) % 80 == 0) std::println();
    }
    std::println();

    if (!g_keystream.empty()) {
        int total = std::min(count, (int)g_keystream.size());
        int matches = 0;
        for (int i = 0; i < total; ++i)
            if (output[i] == g_keystream[i]) ++matches;
        std::println("  Збіг із завантаженою послідовністю z: {}/{} ({:.1f}%)", matches, total, 100.0 * matches / total);
    }
}

static void do_attack_l1() {
    if (g_keystream.empty()) {
        std::println("    Спочатку завантажте послідовність (пункт 2).");
        return;
    }

    auto l1 = make_l1(g_simplified);
    auto params = compute_params(l1.degree(), g_alpha);

    if ((int)g_keystream.size() < params.sequence_length) {
        std::println("    {}  (потрібно {}, є {}).", error_to_string(Error::SequenceTooShort), params.sequence_length, g_keystream.size());
        return;
    }

    std::println("\n  Кореляційна атака на L1 (n={})...", l1.degree());
    std::println("  N* = {},  C = {:.3f},  α = {}", params.sequence_length, params.threshold, g_alpha);

    auto t0 = std::chrono::steady_clock::now();
    g_l1_candidates = find_candidates(l1, g_keystream, params);
    double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();

    std::println("  Завершено за {:.2f} с.  Кандидатів: {}", elapsed, g_l1_candidates.size());

    for (std::size_t i = 0; i < std::min(g_l1_candidates.size(), std::size_t{ 10 }); ++i) {
        uint64_t c = g_l1_candidates[i];
        l1.seed(c);
        int R = 0;
        for (int j = 0; j < params.sequence_length; ++j)
            R += l1.clock() ^ g_keystream[j];
        std::println("    [{}] {:>12}  {}  R = {}", i + 1, c, to_binary(c, l1.degree()), R);
    }
    if (g_l1_candidates.size() > 10)
        std::println("    ... та ще {}.", g_l1_candidates.size() - 10);
}

static void do_attack_l2() {
    if (g_keystream.empty()) {
        std::println("    Спочатку завантажте послідовність (пункт 2).");
        return;
    }

    auto l2 = make_l2(g_simplified);
    auto params = compute_params(l2.degree(), g_alpha);

    if ((int)g_keystream.size() < params.sequence_length) {
        std::println("    {}  (потрібно {}, є {}).", error_to_string(Error::SequenceTooShort), params.sequence_length, g_keystream.size());
        return;
    }

    std::println("\n  Кореляційна атака на L2 (n={})...", l2.degree());
    std::println("  N* = {},  C = {:.3f},  α = {}", params.sequence_length, params.threshold, g_alpha);

    auto t0 = std::chrono::steady_clock::now();
    g_l2_candidates = find_candidates(l2, g_keystream, params);
    double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();

    std::println("  Завершено за {:.2f} с.  Кандидатів: {}", elapsed, g_l2_candidates.size());

    for (std::size_t i = 0; i < std::min(g_l2_candidates.size(), std::size_t{ 10 }); ++i) {
        uint64_t c = g_l2_candidates[i];
        l2.seed(c);
        int R = 0;
        for (int j = 0; j < params.sequence_length; ++j)
            R += l2.clock() ^ g_keystream[j];
        std::println("    [{}] {:>12}  {}  R = {}", i + 1, c, to_binary(c, l2.degree()), R);
    }
    if (g_l2_candidates.size() > 10)
        std::println("    ... та ще {}.", g_l2_candidates.size() - 10);
}

static void do_attack_l3() {
    if (g_l1_candidates.empty() || g_l2_candidates.empty()) {
        std::println("    Спочатку проведіть атаку на L1 і L2 (пункти 5–6).");
        return;
    }

    auto l1 = make_l1(g_simplified);
    auto l2 = make_l2(g_simplified);
    auto l3 = make_l3(g_simplified);
    int  N = (int)g_keystream.size();

    g_key_found = false;
    int pair_index = 0;

    std::println("\n  Атака на L3 (n={}).  Пар L1×L2: {}", l3.degree(), g_l1_candidates.size() * g_l2_candidates.size());

    for (uint64_t s1 : g_l1_candidates) {
        for (uint64_t s2 : g_l2_candidates) {
            l1.seed(s1); l2.seed(s2);
            auto x = l1.clock(N);
            auto y = l2.clock(N);

            std::println("  Пара {}: L1={} L2={}  →  перебір L3", ++pair_index, s1, s2);

            auto t0 = std::chrono::steady_clock::now();
            auto l3_cands = find_l3_candidates(l3, x, y, g_keystream);
            double elapsed = std::chrono::duration<double>(
                std::chrono::steady_clock::now() - t0).count();

            std::println("  Завершено за {:.2f} с.  Кандидатів L3: {}", elapsed, l3_cands.size());

            for (uint64_t s3 : l3_cands) {
                if (verify_key(l1, s1, l2, s2, l3, s3, g_keystream)) {
                    std::println("\n  +--- КЛЮЧ ЗНАЙДЕНО ------------------------------------------------+");
                    std::println("  │  L1 = {:>12}  {}  │", s1, to_binary(s1, l1.degree()));
                    std::println("  │  L2 = {:>12}  {}  │", s2, to_binary(s2, l2.degree()));
                    std::println("  │  L3 = {:>12}  {}  │", s3, to_binary(s3, l3.degree()));
                    std::println("  +------------------------------------------------------------------+");
                    g_key1 = s1; g_key2 = s2; g_key3 = s3;
                    g_key_found = true;
                    return;
                }
            }
        }
    }

    if (!g_key_found)
        std::println("  Ключ не знайдено серед перебраних кандидатів.");
}

static void do_verify() {

    auto l1 = make_l1(g_simplified);
    auto l2 = make_l2(g_simplified);
    auto l3 = make_l3(g_simplified);

    uint64_t s1, s2, s3;

    if (g_key_found) {
        s1 = g_key1; s2 = g_key2; s3 = g_key3;
    }
    else {
        std::println("  Ключ не встановлено. Введіть вручну:");
        auto ask = [](const char* name) -> uint64_t {
            std::print("  L{}: ", name);
            std::cout.flush();
            std::string s;
            std::getline(std::cin, s);
            uint64_t v = 0;
            parse_uint64(s, v);
            return v;
            };
        s1 = ask("1"); s2 = ask("2"); s3 = ask("3");
    }

    if (g_keystream.empty()) {
        std::println("    Послідовність z не завантажена.");
        return;
    }

    std::println("\n  Перевіряємо ключ:");
    std::println("    L1 = {}  ({})", s1, to_binary(s1, l1.degree()));
    std::println("    L2 = {}  ({})", s2, to_binary(s2, l2.degree()));
    std::println("    L3 = {}  ({})", s3, to_binary(s3, l3.degree()));

    bool ok = verify_key(l1, s1, l2, s2, l3, s3, g_keystream);

    if (ok) {
        std::println("\n  OK — гама збігається повністю ({} бітів).", g_keystream.size());
        g_key1 = s1; g_key2 = s2; g_key3 = s3;
        g_key_found = true;
    }
    else {
        GeffeGenerator gen(l1, l2, l3);
        gen.seed(s1, s2, s3);
        int matches = 0;
        for (int zi : g_keystream)
            if (gen.clock() == zi) ++matches;
        std::println("\n  ПОМИЛКА — збігів {}/{} ({:.1f}%).",
            matches, g_keystream.size(),
            100.0 * matches / (double)g_keystream.size());
    }
}

int main() {

    std::string choice;
    while (true) {
        std::println("• • • • • • • • • • • • • • • • • • • • • • • • • • • •");
        std::println("•  1. Режим (спрощений / основний)                    •");
        std::println("•  2. Завантажити послідовність z                     •");
        std::println("•  3. Параметри N* та C                               •");
        std::println("•  4. Тест генератора Джиффі                          •");
        std::println("•  5. Кореляційна атака на L1                         •");
        std::println("•  6. Кореляційна атака на L2                         •");
        std::println("•  7. Атака на L3 за знайденими кандидатами           •");
        std::println("•  8. Перевірка ключа                                 •");
        std::println("•  q. Вихід                                           •");
        std::println("• • • • • • • • • • • • • • • • • • • • • • • • • • • •");

        if (!g_keystream.empty())
            std::println("    z: {} бітів  │  режим: {}  │  ключ: {}", g_keystream.size(), g_simplified ? "спрощений" : "основний", g_key_found ? std::format("L1={} L2={} L3={}", g_key1, g_key2, g_key3) : "не знайдено");

        std::print("Вибір: ");
        std::cout.flush();

        if (!std::getline(std::cin, choice) || choice == "q" || choice == "Q") {
            std::println("BYE BYE!");
            break;
        }
        if (choice.empty()) continue;

        std::println();
        switch (choice[0]) {
        case '1': do_mode();        break;
        case '2': do_load();        break;
        case '3': do_show_params(); break;
        case '4': do_geffe_test();  break;
        case '5': do_attack_l1();   break;
        case '6': do_attack_l2();   break;
        case '7': do_attack_l3();   break;
        case '8': do_verify();      break;
        default:  std::println("  Невідома команда."); break;
        }
        std::println();
    }
    return 0;
}