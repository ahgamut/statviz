#pragma once

#include "statviz/families.hpp"

// Per-curve data for the GPU: two RGBA32F texels per curve.
//   texel 0: family constants (see pack_family; web/shaders must match)
//   texel 1: (u_norm, v_norm, valid ? 1 : 0, 0)
namespace sv {

inline constexpr int kTexelsPerCurve = 2;
inline constexpr int kCurveFloats = kTexelsPerCurve * 4;

// Writes texel 0 from validated params, computed in double.
//   normal        (mu, sigma^2/2)          exponential  (1/lambda)
//   gamma         (k, theta)               chi_squared  (k/2, 2)      [gamma layout]
//   uniform       ((a+b)/2, (b-a)/2)       laplace      (mu, b^2)
//   cauchy        (x0, gamma)              logistic     (mu, pi s)
//   stable        (alpha, beta*T, gamma, delta), T = tan(pi alpha/2), or 2/pi at alpha = 1
//   bernoulli     (p)                      binomial     (n, p)
//   poisson       (lambda)                 geometric    (p)
//   neg_binomial  (r, p)                   discrete_uniform ((a+b)/2, b-a+1)
void pack_family(const Family& f, const double* p, float* texel);

}  // namespace sv
