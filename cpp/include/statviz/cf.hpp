#pragma once

#include <cmath>
#include <complex>

// Characteristic functions phi(t) = E[exp(i t X)], double-precision reference.
namespace sv::cf {

using cplx = std::complex<double>;

// Normal(mu, sigma): exp(i mu t - sigma^2 t^2 / 2)
inline cplx normal(double t, double mu, double sigma) {
    return std::polar(std::exp(-0.5 * sigma * sigma * t * t), mu * t);
}

}  // namespace sv::cf
