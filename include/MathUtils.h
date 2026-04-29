#pragma once

struct CorrelationParams {
    int sequence_length = 0;
    double threshold = 0;
    double beta = 0;
    double t_alpha = 0;
    double t_beta = 0; 
};

CorrelationParams compute_params(int degree, double alpha);