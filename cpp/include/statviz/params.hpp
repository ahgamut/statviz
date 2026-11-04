#pragma once

// Per-curve constants uploaded to the GPU, one RGBA32F texel per curve.
namespace sv {

inline constexpr int kTexelFloats = 4;

// Normal: (mu, sigma^2 / 2, 0, 0)
void normal_texel(double mu, double sigma, float* out);

// n curves with sigma linear in [s0, s1]; out holds n texels.
void normal_sweep(double mu, double s0, double s1, int n, float* out);

}  // namespace sv
