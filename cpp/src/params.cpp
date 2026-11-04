#include "statviz/params.hpp"

namespace sv {

void normal_texel(double mu, double sigma, float* out) {
    out[0] = static_cast<float>(mu);
    out[1] = static_cast<float>(0.5 * sigma * sigma);
    out[2] = 0.0f;
    out[3] = 0.0f;
}

void normal_sweep(double mu, double s0, double s1, int n, float* out) {
    for (int i = 0; i < n; ++i) {
        double f = n > 1 ? double(i) / (n - 1) : 0.0;
        normal_texel(mu, s0 + f * (s1 - s0), out + i * kTexelFloats);
    }
}

}  // namespace sv
