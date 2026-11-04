#include "statviz/families.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace sv {

namespace {

using cf::cplx;

// Smallest value used when clamping to an open lower bound.
constexpr double kTiny = 1e-9;

constexpr Family kFamilies[] = {
    {FamilyId::Normal, "normal", "Normal", false, false,
     R"(\varphi(t)=e^{i\mu t-\sigma^2t^2/2})", 2,
     {{"mu", "μ", Domain::Real, 0.0}, {"sigma", "σ", Domain::NonNegative, 1.0}},
     [](const double* p, double t) { return cf::normal(t, p[0], p[1]); }},
    {FamilyId::Exponential, "exponential", "Exponential", false, false,
     R"(\varphi(t)=\left(1-\frac{it}{\lambda}\right)^{-1})", 1,
     {{"lambda", "λ", Domain::Positive, 1.0}},
     [](const double* p, double t) { return cf::exponential(t, p[0]); }},
    {FamilyId::Gamma, "gamma", "Gamma", false, false,
     R"(\varphi(t)=(1-i\theta t)^{-k})", 2,
     {{"k", "k", Domain::NonNegative, 2.0}, {"theta", "θ", Domain::NonNegative, 1.0}},
     [](const double* p, double t) { return cf::gamma(t, p[0], p[1]); }},
    {FamilyId::ChiSquared, "chi_squared", "Chi-squared", false, false,
     R"(\varphi(t)=(1-2it)^{-k/2})", 1,
     {{"k", "k", Domain::NonNegative, 3.0}},
     [](const double* p, double t) { return cf::chi_squared(t, p[0]); }},
    {FamilyId::Uniform, "uniform", "Uniform", false, true,
     R"(\varphi(t)=\frac{e^{ibt}-e^{iat}}{it(b-a)})", 2,
     {{"a", "a", Domain::Real, 0.0}, {"b", "b", Domain::Real, 1.0}},
     [](const double* p, double t) { return cf::uniform(t, p[0], p[1]); }},
    {FamilyId::Laplace, "laplace", "Laplace", false, false,
     R"(\varphi(t)=\frac{e^{i\mu t}}{1+b^2t^2})", 2,
     {{"mu", "μ", Domain::Real, 0.0}, {"b", "b", Domain::NonNegative, 1.0}},
     [](const double* p, double t) { return cf::laplace(t, p[0], p[1]); }},
    {FamilyId::Cauchy, "cauchy", "Cauchy", false, false,
     R"(\varphi(t)=e^{ix_0t-\gamma|t|})", 2,
     {{"x0", "x₀", Domain::Real, 0.0}, {"gamma", "γ", Domain::NonNegative, 1.0}},
     [](const double* p, double t) { return cf::cauchy(t, p[0], p[1]); }},
    {FamilyId::Logistic, "logistic", "Logistic", false, false,
     R"(\varphi(t)=e^{i\mu t}\,\frac{\pi st}{\sinh(\pi st)})", 2,
     {{"mu", "μ", Domain::Real, 0.0}, {"s", "s", Domain::NonNegative, 1.0}},
     [](const double* p, double t) { return cf::logistic(t, p[0], p[1]); }},
    {FamilyId::Stable, "stable", "Stable (S0)", false, false,
     R"(\varphi(t)=\exp\!\Big(i\delta t-|\gamma t|^{\alpha}\big[1+i\beta\tan\tfrac{\pi\alpha}{2}\,\operatorname{sgn}(t)\,(|\gamma t|^{1-\alpha}-1)\big]\Big))",
     4,
     {{"alpha", "α", Domain::StableAlpha, 1.5},
      {"beta", "β", Domain::Skew, 0.5},
      {"gamma", "γ", Domain::NonNegative, 1.0},
      {"delta", "δ", Domain::Real, 0.0}},
     [](const double* p, double t) { return cf::stable(t, p[0], p[1], p[2], p[3]); }},
    {FamilyId::Bernoulli, "bernoulli", "Bernoulli", true, false,
     R"(\varphi(t)=1-p+pe^{it})", 1,
     {{"p", "p", Domain::Unit, 0.5}},
     [](const double* p, double t) { return cf::bernoulli(t, p[0]); }},
    {FamilyId::Binomial, "binomial", "Binomial", true, false,
     R"(\varphi(t)=(1-p+pe^{it})^n)", 2,
     {{"n", "n", Domain::Count, 10.0}, {"p", "p", Domain::Unit, 0.3}},
     [](const double* p, double t) { return cf::binomial(t, p[0], p[1]); }},
    {FamilyId::Poisson, "poisson", "Poisson", true, false,
     R"(\varphi(t)=e^{\lambda(e^{it}-1)})", 1,
     {{"lambda", "λ", Domain::NonNegative, 3.0}},
     [](const double* p, double t) { return cf::poisson(t, p[0]); }},
    {FamilyId::Geometric, "geometric", "Geometric", true, false,
     R"(\varphi(t)=\frac{p}{1-(1-p)e^{it}})", 1,
     {{"p", "p", Domain::UnitPositive, 0.3}},
     [](const double* p, double t) { return cf::geometric(t, p[0]); }},
    {FamilyId::NegBinomial, "neg_binomial", "Negative binomial", true, false,
     R"(\varphi(t)=\left(\frac{p}{1-(1-p)e^{it}}\right)^{r})", 2,
     {{"r", "r", Domain::Positive, 3.0}, {"p", "p", Domain::UnitPositive, 0.4}},
     [](const double* p, double t) { return cf::neg_binomial(t, p[0], p[1]); }},
    {FamilyId::DiscreteUniform, "discrete_uniform", "Discrete uniform", true, true,
     R"(\varphi(t)=\frac{e^{iat}-e^{i(b+1)t}}{(b-a+1)(1-e^{it})})", 2,
     {{"a", "a", Domain::Integer, 0.0}, {"b", "b", Domain::Integer, 5.0}},
     [](const double* p, double t) { return cf::discrete_uniform(t, p[0], p[1]); }},
};

static_assert(std::size(kFamilies) == static_cast<size_t>(FamilyId::DiscreteUniform) + 1);

bool in_domain(Domain d, double x) {
    switch (d) {
        case Domain::Real: case Domain::Integer: return true;
        case Domain::NonNegative: case Domain::Count: return x >= 0.0;
        case Domain::Positive: return x > 0.0;
        case Domain::Unit: return x >= 0.0 && x <= 1.0;
        case Domain::UnitPositive: return x > 0.0 && x <= 1.0;
        case Domain::StableAlpha: return x > 0.0 && x <= 2.0;
        case Domain::Skew: return x >= -1.0 && x <= 1.0;
    }
    return false;
}

double clamp_to(Domain d, double x) {
    switch (d) {
        case Domain::Real: case Domain::Integer: return x;
        case Domain::NonNegative: case Domain::Count: return std::max(x, 0.0);
        case Domain::Positive: return std::max(x, kTiny);
        case Domain::Unit: return std::clamp(x, 0.0, 1.0);
        case Domain::UnitPositive: return std::clamp(x, kTiny, 1.0);
        case Domain::StableAlpha: return std::clamp(x, kTiny, 2.0);
        case Domain::Skew: return std::clamp(x, -1.0, 1.0);
    }
    return x;
}

}  // namespace

const char* domain_name(Domain d) {
    switch (d) {
        case Domain::Real: return "real";
        case Domain::NonNegative: return ">= 0";
        case Domain::Positive: return "> 0";
        case Domain::Unit: return "in [0, 1]";
        case Domain::UnitPositive: return "in (0, 1]";
        case Domain::Count: return "integer >= 0";
        case Domain::Integer: return "integer";
        case Domain::StableAlpha: return "in (0, 2]";
        case Domain::Skew: return "in [-1, 1]";
    }
    return "?";
}

std::span<const Family> families() { return kFamilies; }

const Family* find_family(int id) {
    if (id < 0 || id >= static_cast<int>(std::size(kFamilies))) return nullptr;
    return &kFamilies[id];
}

unsigned validate(const Family& f, double* p, InvalidPolicy policy) {
    unsigned bad = 0;
    for (int i = 0; i < f.nparams; ++i) {
        Domain d = f.params[i].domain;
        if (!std::isfinite(p[i])) {
            bad |= 1u << i;
            continue;
        }
        if (d == Domain::Count || d == Domain::Integer) p[i] = std::round(p[i]);
        if (!in_domain(d, p[i])) {
            bad |= 1u << i;
            if (policy == InvalidPolicy::Clamp) p[i] = clamp_to(d, p[i]);
        }
    }
    if (f.ordered_pair && std::isfinite(p[0]) && std::isfinite(p[1]) && p[0] > p[1]) {
        bad |= 0b11;
        if (policy == InvalidPolicy::Clamp) std::swap(p[0], p[1]);
    }
    return bad;
}

bool is_valid(const Family& f, const double* p) {
    for (int i = 0; i < f.nparams; ++i)
        if (!std::isfinite(p[i]) || !in_domain(f.params[i].domain, p[i])) return false;
    return !f.ordered_pair || p[0] <= p[1];
}

}  // namespace sv
