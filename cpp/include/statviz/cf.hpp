#pragma once

#include <cmath>
#include <complex>
#include <numbers>

// Characteristic functions phi(t) = E[exp(i t X)], double-precision reference.
// Forms are chosen to stay accurate at large |t| and near degenerate limits.
namespace sv::cf {

using cplx = std::complex<double>;

// r * exp(i a); r may be negative.
inline cplx cis(double a, double r = 1.0) { return {r * std::cos(a), r * std::sin(a)}; }

// x / sinh(x), finite for all x.
inline double x_over_sinh(double x) {
    double ax = std::abs(x);
    if (ax < 1e-4) return 1.0 - ax * ax / 6.0;
    return 2.0 * ax * std::exp(-ax) / -std::expm1(-2.0 * ax);
}

// sin(x) / x
inline double sinc(double x) {
    return std::abs(x) < 1e-4 ? 1.0 - x * x / 6.0 : std::sin(x) / x;
}

// ---- continuous ----

// Normal(mu, sigma >= 0)
inline cplx normal(double t, double mu, double sigma) {
    return std::polar(std::exp(-0.5 * sigma * sigma * t * t), mu * t);
}

// Exponential(rate lambda > 0): 1 / (1 - i t / lambda)
inline cplx exponential(double t, double lambda) {
    double x = t / lambda;
    return cplx(1.0, x) / (1.0 + x * x);
}

// Gamma(shape k >= 0, scale theta >= 0): (1 - i theta t)^-k
inline cplx gamma(double t, double k, double theta) {
    double x = theta * t;
    return std::polar(std::exp(-0.5 * k * std::log1p(x * x)), k * std::atan(x));
}

// Chi-squared(k >= 0) = Gamma(k/2, 2)
inline cplx chi_squared(double t, double k) { return gamma(t, 0.5 * k, 2.0); }

// Uniform on [a, b], a <= b
inline cplx uniform(double t, double a, double b) {
    return cis(0.5 * (a + b) * t, sinc(0.5 * (b - a) * t));
}

// Laplace(mu, scale b >= 0)
inline cplx laplace(double t, double mu, double b) {
    return std::polar(1.0 / (1.0 + b * b * t * t), mu * t);
}

// Cauchy(location x0, scale gamma >= 0)
inline cplx cauchy(double t, double x0, double g) {
    return std::polar(std::exp(-g * std::abs(t)), x0 * t);
}

// Logistic(mu, scale s >= 0)
inline cplx logistic(double t, double mu, double s) {
    return std::polar(x_over_sinh(std::numbers::pi * s * t), mu * t);
}

// Stable(alpha in (0,2], beta in [-1,1], gamma >= 0, delta), Nolan's S0
// parameterization: continuous in alpha, including alpha = 1.
inline cplx stable(double t, double alpha, double beta, double g, double delta) {
    double gt = std::abs(g * t);
    if (gt == 0.0) return cis(delta * t);
    double sgn = t > 0 ? 1.0 : -1.0;
    double re = -std::pow(gt, alpha);
    double im;
    if (alpha == 1.0) {
        im = -gt * beta * (2.0 / std::numbers::pi) * sgn * std::log(gt);
    } else {
        // tan(pi a / 2) (|gt|^(1-a) - 1), with expm1 for accuracy near a = 1
        double k = std::tan(0.5 * std::numbers::pi * alpha) * std::expm1((1.0 - alpha) * std::log(gt));
        im = re * beta * k * sgn;
    }
    return std::polar(std::exp(re), im + delta * t);
}

// ---- discrete (integer support, so phi has period 2 pi) ----

// Bernoulli(p in [0,1])
inline cplx bernoulli(double t, double p) { return cplx(1.0 - p, 0.0) + cis(t, p); }

// Binomial(n >= 0 integer, p in [0,1])
inline cplx binomial(double t, double n, double p) {
    cplx z = bernoulli(t, p);
    if (n == 0.0) return 1.0;
    if (z == 0.0) return 0.0;
    return std::exp(n * std::log(z));
}

// Poisson(lambda >= 0); cos t - 1 = -2 sin^2(t/2)
inline cplx poisson(double t, double lambda) {
    double s = std::sin(0.5 * t);
    return std::polar(std::exp(-2.0 * lambda * s * s), lambda * std::sin(t));
}

// Geometric(p in (0,1]), number of failures before the first success.
// 1 - (1-p) e^{it} = p + (1-p)(1 - cos t) - i (1-p) sin t, exact at t = 0.
inline cplx geometric(double t, double p) {
    double q = 1.0 - p, s = std::sin(0.5 * t);
    return p / cplx(p + 2.0 * q * s * s, -q * std::sin(t));
}

// Negative binomial(r > 0, p in (0,1]), failures before the r-th success
inline cplx neg_binomial(double t, double r, double p) {
    return std::exp(r * std::log(geometric(t, p)));
}

// Discrete uniform on the integers a..b, a <= b
inline cplx discrete_uniform(double t, double a, double b) {
    double n = b - a + 1.0;
    double s = std::sin(0.5 * t);
    double d = std::abs(s) < 1e-9 ? std::cos(0.5 * n * t) / std::cos(0.5 * t)
                                   : std::sin(0.5 * n * t) / (n * s);
    return cis(0.5 * (a + b) * t, d);
}

}  // namespace sv::cf
