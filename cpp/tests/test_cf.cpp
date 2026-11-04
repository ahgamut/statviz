#include <cmath>
#include <numbers>

#include "doctest.h"
#include "statviz/cf.hpp"

using sv::cf::cplx;

namespace {

// E[exp(itX)] by composite Simpson over [mu - 12 sigma, mu + 12 sigma].
cplx normal_by_quadrature(double t, double mu, double sigma) {
    const int n = 20000;
    const double a = mu - 12 * sigma, b = mu + 12 * sigma, h = (b - a) / n;
    cplx sum = 0;
    for (int k = 0; k <= n; ++k) {
        double x = a + k * h;
        double z = (x - mu) / sigma;
        double pdf = std::exp(-0.5 * z * z) / (sigma * std::sqrt(2 * std::numbers::pi));
        double w = (k == 0 || k == n) ? 1 : (k % 2 ? 4 : 2);
        sum += w * pdf * std::polar(1.0, t * x);
    }
    return sum * (h / 3);
}

const double kMus[] = {-3.0, 0.0, 0.5, 7.0};
const double kSigmas[] = {0.01, 0.3, 1.0, 4.0};
const double kTs[] = {-20.0, -1.5, -0.1, 0.0, 0.1, 1.0, 2.5, 40.0};

}  // namespace

TEST_CASE("normal: phi(0) = 1, |phi| <= 1, phi(-t) = conj(phi(t))") {
    for (double mu : kMus)
        for (double s : kSigmas) {
            CHECK(std::abs(sv::cf::normal(0, mu, s) - 1.0) == 0.0);
            for (double t : kTs) {
                cplx p = sv::cf::normal(t, mu, s);
                CHECK(std::abs(p) <= 1.0);
                CHECK(std::abs(sv::cf::normal(-t, mu, s) - std::conj(p)) < 1e-15);
            }
        }
}

TEST_CASE("normal: closed form at mu = 1, sigma = 1, t = 1") {
    cplx p = sv::cf::normal(1, 1, 1);
    CHECK(p.real() == doctest::Approx(std::exp(-0.5) * std::cos(1.0)).epsilon(1e-15));
    CHECK(p.imag() == doctest::Approx(std::exp(-0.5) * std::sin(1.0)).epsilon(1e-15));
}

TEST_CASE("normal: matches numerical E[exp(itX)]") {
    for (double mu : {-2.0, 0.0, 1.5})
        for (double s : {0.3, 1.0, 2.0})
            for (double t : {-3.0, -0.5, 0.7, 2.0}) {
                cplx diff = sv::cf::normal(t, mu, s) - normal_by_quadrature(t, mu, s);
                CHECK(std::abs(diff) < 1e-10);
            }
}
