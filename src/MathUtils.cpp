#include "../include/MathUtils.h"
#include <cmath>


static double normal_ppf(double p) {
    if (p <= 0.0) return -10.0;
    if (p >= 1.0) return  10.0;
    if (p < 0.5) return -normal_ppf(1.0 - p);

    double low = 0.0, high = 9.0;

    for (int i = 0; i < 100; ++i) {
        double mid = (low + high) * 0.5;
        double cdf = 1.0 - 0.5 * std::erfc(mid / std::sqrt(2.0));
        (cdf < p ? low : high) = mid;
    }
    return (low + high) * 0.5;
}

CorrelationParams compute_params(int degree, double alpha) {

    constexpr double p1 = 0.25;
    constexpr double p2 = 0.50;

    double t_alpha = normal_ppf(1.0 - alpha);

    double t_beta = normal_ppf(1.0 - std::ldexp(1.0, -degree));


    double sqrt_N = 2.0 * t_beta + t_alpha * std::sqrt(3.0);
    int    N_star = static_cast<int>(std::ceil(sqrt_N * sqrt_N));

    double C = N_star * p1 + t_alpha * std::sqrt(N_star * p1 * (1.0 - p1));

    double t_beta_actual = (N_star * p2 - C) / std::sqrt(N_star * p2 * (1.0 - p2));
    double beta_actual = 0.5 * std::erfc(t_beta_actual / std::sqrt(2.0));

    return { N_star, C, beta_actual, t_alpha, t_beta_actual };
}