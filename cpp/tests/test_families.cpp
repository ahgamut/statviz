#include <cmath>
#include <functional>
#include <numbers>
#include <set>
#include <string>
#include <vector>

#include "doctest.h"
#include "statviz/families.hpp"

using sv::Family;
using sv::FamilyId;
using sv::cf::cplx;

namespace {

constexpr double kPi = std::numbers::pi;

const Family& fam(FamilyId id) { return *sv::find_family(static_cast<int>(id)); }

cplx phi(FamilyId id, std::vector<double> p, double t) { return fam(id).phi(p.data(), t); }

struct Case {
    FamilyId id;
    std::vector<double> p;
};

// Valid parameter sets, including degenerate and boundary values.
const std::vector<Case> kCases = {
    {FamilyId::Normal, {0, 1}}, {FamilyId::Normal, {3.5, 0.2}}, {FamilyId::Normal, {-1, 0}},
    {FamilyId::Exponential, {1}}, {FamilyId::Exponential, {0.05}}, {FamilyId::Exponential, {40}},
    {FamilyId::Gamma, {0.3, 2}}, {FamilyId::Gamma, {7, 0.5}}, {FamilyId::Gamma, {0, 1}},
    {FamilyId::ChiSquared, {1}}, {FamilyId::ChiSquared, {12}},
    {FamilyId::Uniform, {0, 1}}, {FamilyId::Uniform, {-3, 0.5}}, {FamilyId::Uniform, {2, 2}},
    {FamilyId::Laplace, {1, 0.5}}, {FamilyId::Laplace, {0, 0}},
    {FamilyId::Cauchy, {2, 0.3}}, {FamilyId::Cauchy, {-1, 0}},
    {FamilyId::Logistic, {0.5, 1}}, {FamilyId::Logistic, {0, 0}}, {FamilyId::Logistic, {-2, 3}},
    {FamilyId::Stable, {1.5, 0.5, 1, 0}}, {FamilyId::Stable, {0.5, -1, 2, 1}},
    {FamilyId::Stable, {1, 0.8, 1, 0.3}}, {FamilyId::Stable, {0.999, 0.8, 1, 0}},
    {FamilyId::Stable, {2, 1, 0.7, -1}}, {FamilyId::Stable, {1.2, 0, 0, 2}},
    {FamilyId::Bernoulli, {0}}, {FamilyId::Bernoulli, {0.3}}, {FamilyId::Bernoulli, {1}},
    {FamilyId::Binomial, {10, 0.3}}, {FamilyId::Binomial, {0, 0.5}}, {FamilyId::Binomial, {2000, 0.7}},
    {FamilyId::Poisson, {0}}, {FamilyId::Poisson, {3}}, {FamilyId::Poisson, {250}},
    {FamilyId::Geometric, {1}}, {FamilyId::Geometric, {0.3}}, {FamilyId::Geometric, {0.01}},
    {FamilyId::NegBinomial, {3, 0.4}}, {FamilyId::NegBinomial, {0.5, 0.9}}, {FamilyId::NegBinomial, {40, 0.2}},
    {FamilyId::DiscreteUniform, {0, 5}}, {FamilyId::DiscreteUniform, {-3, 3}}, {FamilyId::DiscreteUniform, {4, 4}},
};

std::vector<double> t_grid() {
    std::vector<double> ts;
    for (double t = -60; t <= 60; t += 0.37) ts.push_back(t);
    for (double t : {1e-12, 1e-6, 1e-3, kPi, 2 * kPi, 100 * kPi, 1e3, 1e5}) {
        ts.push_back(t);
        ts.push_back(-t);
    }
    return ts;
}

std::string label(const Case& c) {
    std::string s = fam(c.id).key;
    for (double x : c.p) s += " " + std::to_string(x);
    return s;
}

// Composite Simpson of pdf(x) exp(itx) over [a, b].
cplx expect_continuous(const std::function<double(double)>& pdf, double a, double b, double t) {
    const int n = 40000;
    double h = (b - a) / n;
    cplx sum = 0;
    for (int k = 0; k <= n; ++k) {
        double x = a + k * h;
        double w = (k == 0 || k == n) ? 1 : (k % 2 ? 4 : 2);
        sum += w * pdf(x) * sv::cf::cis(t * x);
    }
    return sum * (h / 3);
}

// sum_k pmf(k) exp(itk) for k in [lo, hi].
cplx expect_discrete(const std::function<double(double)>& pmf, int lo, int hi, double t) {
    cplx sum = 0;
    for (int k = lo; k <= hi; ++k) sum += pmf(k) * sv::cf::cis(t * k);
    return sum;
}

double log_choose(double n, double k) {
    return std::lgamma(n + 1) - std::lgamma(k + 1) - std::lgamma(n - k + 1);
}

}  // namespace

TEST_CASE("family table is consistent") {
    std::set<std::string> keys;
    int i = 0;
    for (const Family& f : sv::families()) {
        CHECK(static_cast<int>(f.id) == i);
        CHECK(sv::find_family(i) == &f);
        CHECK(keys.insert(f.key).second);
        CHECK(f.nparams >= 1);
        CHECK(f.nparams <= sv::kMaxParams);
        for (int j = 0; j < f.nparams; ++j) {
            CHECK(f.params[j].name != nullptr);
            double p[sv::kMaxParams];
            for (int k = 0; k < f.nparams; ++k) p[k] = f.params[k].fallback;
            CHECK(sv::is_valid(f, p));
        }
        ++i;
    }
    CHECK(sv::find_family(-1) == nullptr);
    CHECK(sv::find_family(i) == nullptr);
}

TEST_CASE("characteristic function properties") {
    const auto ts = t_grid();
    for (const Case& c : kCases) {
        CAPTURE(label(c));
        const Family& f = fam(c.id);
        REQUIRE(sv::is_valid(f, c.p.data()));
        CHECK(std::abs(f.phi(c.p.data(), 0.0) - 1.0) < 1e-14);
        for (double t : ts) {
            CAPTURE(t);
            cplx z = f.phi(c.p.data(), t);
            CHECK(std::isfinite(z.real()));
            CHECK(std::isfinite(z.imag()));
            CHECK(std::abs(z) <= 1.0 + 1e-12);
            CHECK(std::abs(f.phi(c.p.data(), -t) - std::conj(z)) < 1e-9);
            if (f.lattice && std::abs(t) < 1e3)
                CHECK(std::abs(f.phi(c.p.data(), t + 2 * kPi) - z) < 1e-9);
        }
    }
}

TEST_CASE("identities between families") {
    for (double t : {-7.0, -0.4, 0.3, 1.0, 2.5, 30.0}) {
        CAPTURE(t);
        CHECK(std::abs(phi(FamilyId::ChiSquared, {5}, t) - phi(FamilyId::Gamma, {2.5, 2}, t)) < 1e-14);
        CHECK(std::abs(phi(FamilyId::Binomial, {1, 0.3}, t) - phi(FamilyId::Bernoulli, {0.3}, t)) < 1e-14);
        CHECK(std::abs(phi(FamilyId::NegBinomial, {1, 0.3}, t) - phi(FamilyId::Geometric, {0.3}, t)) < 1e-14);
        // Stable: alpha = 2 is Normal(delta, sqrt(2) gamma); alpha = 1, beta = 0 is Cauchy.
        CHECK(std::abs(phi(FamilyId::Stable, {2, 0.9, 0.7, -1}, t) -
                       phi(FamilyId::Normal, {-1, std::sqrt(2.0) * 0.7}, t)) < 1e-12);
        CHECK(std::abs(phi(FamilyId::Stable, {1, 0, 0.7, 2}, t) - phi(FamilyId::Cauchy, {2, 0.7}, t)) < 1e-14);
        // S0 is continuous in alpha at 1.
        cplx at1 = phi(FamilyId::Stable, {1, 0.8, 1.3, 0.5}, t);
        for (double eps : {1e-6, -1e-6})
            CHECK(std::abs(phi(FamilyId::Stable, {1 + eps, 0.8, 1.3, 0.5}, t) - at1) < 1e-4);
        // Degenerate limits are point masses.
        CHECK(std::abs(phi(FamilyId::Normal, {1.5, 0}, t) - sv::cf::cis(1.5 * t)) < 1e-14);
        CHECK(std::abs(phi(FamilyId::Uniform, {1.5, 1.5}, t) - sv::cf::cis(1.5 * t)) < 1e-14);
        CHECK(std::abs(phi(FamilyId::DiscreteUniform, {3, 3}, t) - sv::cf::cis(3 * t)) < 1e-12);
        CHECK(std::abs(phi(FamilyId::Logistic, {-2, 0}, t) - sv::cf::cis(-2 * t)) < 1e-14);
    }
}

TEST_CASE("continuous families match E[exp(itX)] by quadrature") {
    const double sq2pi = std::sqrt(2 * kPi);
    struct Q {
        FamilyId id;
        std::vector<double> p;
        std::function<double(double)> pdf;
        double a, b;
    };
    const std::vector<Q> qs = {
        {FamilyId::Normal, {0.7, 1.3}, [=](double x) { double z = (x - 0.7) / 1.3; return std::exp(-0.5 * z * z) / (1.3 * sq2pi); }, -15, 16},
        {FamilyId::Exponential, {2}, [](double x) { return 2 * std::exp(-2 * x); }, 0, 25},
        {FamilyId::Gamma, {3.5, 0.8}, [](double x) { return std::exp(2.5 * std::log(x) - x / 0.8 - std::lgamma(3.5) - 3.5 * std::log(0.8)); }, 1e-300, 60},
        {FamilyId::ChiSquared, {4}, [](double x) { return x * std::exp(-x / 2) / 4; }, 0, 120},
        {FamilyId::Uniform, {-1, 2.5}, [](double) { return 1 / 3.5; }, -1, 2.5},
        {FamilyId::Laplace, {0.5, 0.6}, [](double x) { return std::exp(-std::abs(x - 0.5) / 0.6) / 1.2; }, -30, 31},
        {FamilyId::Logistic, {-1, 0.4}, [](double x) { double e = std::exp(-(x + 1) / 0.4); return e / (0.4 * (1 + e) * (1 + e)); }, -30, 28},
    };
    for (const Q& q : qs) {
        CAPTURE(fam(q.id).key);
        for (double t : {-3.0, -0.6, 0.25, 1.0, 2.2}) {
            CAPTURE(t);
            CHECK(std::abs(phi(q.id, q.p, t) - expect_continuous(q.pdf, q.a, q.b, t)) < 1e-7);
        }
    }
}

TEST_CASE("discrete families match E[exp(itX)] by summing the pmf") {
    struct D {
        FamilyId id;
        std::vector<double> p;
        std::function<double(double)> pmf;
        int lo, hi;
    };
    const std::vector<D> ds = {
        {FamilyId::Bernoulli, {0.3}, [](double k) { return k == 1 ? 0.3 : 0.7; }, 0, 1},
        {FamilyId::Binomial, {12, 0.35}, [](double k) { return std::exp(log_choose(12, k) + k * std::log(0.35) + (12 - k) * std::log(0.65)); }, 0, 12},
        {FamilyId::Poisson, {4.5}, [](double k) { return std::exp(k * std::log(4.5) - 4.5 - std::lgamma(k + 1)); }, 0, 80},
        {FamilyId::Geometric, {0.3}, [](double k) { return 0.3 * std::pow(0.7, k); }, 0, 400},
        {FamilyId::NegBinomial, {2.5, 0.4}, [](double k) { return std::exp(std::lgamma(k + 2.5) - std::lgamma(k + 1) - std::lgamma(2.5) + 2.5 * std::log(0.4) + k * std::log(0.6)); }, 0, 400},
        {FamilyId::DiscreteUniform, {-2, 4}, [](double) { return 1.0 / 7; }, -2, 4},
    };
    for (const D& d : ds) {
        CAPTURE(fam(d.id).key);
        for (double t : {-3.0, -0.6, 0.25, 1.0, 2.2, 9.0}) {
            CAPTURE(t);
            CHECK(std::abs(phi(d.id, d.p, t) - expect_discrete(d.pmf, d.lo, d.hi, t)) < 1e-12);
        }
    }
}

TEST_CASE("validate: skip and clamp policies") {
    using sv::InvalidPolicy;
    const Family& normal = fam(FamilyId::Normal);

    double p[] = {1.0, -0.5};
    CHECK(sv::validate(normal, p, InvalidPolicy::Skip) == 0b10);
    CHECK(p[1] == -0.5);
    CHECK_FALSE(sv::is_valid(normal, p));

    CHECK(sv::validate(normal, p, InvalidPolicy::Clamp) == 0b10);
    CHECK(p[1] == 0.0);
    CHECK(sv::is_valid(normal, p));

    double q[] = {NAN, 1.0};
    CHECK(sv::validate(normal, q, InvalidPolicy::Clamp) == 0b01);
    CHECK_FALSE(sv::is_valid(normal, q));

    double n[] = {2.6, 0.3};
    CHECK(sv::validate(fam(FamilyId::Binomial), n, InvalidPolicy::Skip) == 0);
    CHECK(n[0] == 3.0);

    double u[] = {2.0, -1.0};
    CHECK(sv::validate(fam(FamilyId::Uniform), u, InvalidPolicy::Skip) == 0b11);
    CHECK_FALSE(sv::is_valid(fam(FamilyId::Uniform), u));
    CHECK(sv::validate(fam(FamilyId::Uniform), u, InvalidPolicy::Clamp) == 0b11);
    CHECK(u[0] == -1.0);
    CHECK(u[1] == 2.0);

    double g[] = {0.0};
    CHECK(sv::validate(fam(FamilyId::Geometric), g, InvalidPolicy::Clamp) == 0b1);
    CHECK(g[0] > 0.0);
    CHECK(sv::is_valid(fam(FamilyId::Geometric), g));

    double s[] = {2.5, -3.0, 1.0, 0.0};
    CHECK(sv::validate(fam(FamilyId::Stable), s, InvalidPolicy::Clamp) == 0b011);
    CHECK(s[0] == 2.0);
    CHECK(s[1] == -1.0);
}
