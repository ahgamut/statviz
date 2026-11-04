#pragma once

#include <span>

#include "statviz/cf.hpp"

// Distribution families: parameter metadata, validation and phi(t).
namespace sv {

inline constexpr int kMaxParams = 4;

enum class Domain {
    Real,         // finite
    NonNegative,  // >= 0 (0 gives the degenerate limit)
    Positive,     // > 0
    Unit,         // [0, 1]
    UnitPositive, // (0, 1]
    Count,        // integer >= 0, rounded
    Integer,      // rounded
    StableAlpha,  // (0, 2]
    Skew,         // [-1, 1]
};

const char* domain_name(Domain d);

struct ParamSpec {
    const char* name;    // ASCII identifier, e.g. "sigma"
    const char* symbol;  // UTF-8 display symbol, e.g. "σ"
    Domain domain;
    double fallback;     // default value
};

enum class FamilyId : int {
    Normal, Exponential, Gamma, ChiSquared, Uniform, Laplace, Cauchy, Logistic, Stable,
    Bernoulli, Binomial, Poisson, Geometric, NegBinomial, DiscreteUniform,
};

struct Family {
    FamilyId id;
    const char* key;    // stable identifier, e.g. "normal"
    const char* name;   // display name
    bool lattice;       // integer support: phi has period 2 pi
    bool ordered_pair;  // params 0 and 1 are an interval's ends (a <= b)
    const char* latex;
    int nparams;
    ParamSpec params[kMaxParams];
    cf::cplx (*phi)(const double* p, double t);
};

std::span<const Family> families();
const Family* find_family(int id);

enum class InvalidPolicy { Skip, Clamp };

// Brings p into the family's domain. Count/Integer params are always
// rounded. Returns a bitmask of params that were out of domain: with
// Clamp they are clamped (an inverted interval is swapped), with Skip the
// curve should be dropped. Non-finite values are always reported and,
// even with Clamp, leave the curve invalid (see is_valid).
unsigned validate(const Family& f, double* p, InvalidPolicy policy);

// True if p (already validated) can be evaluated.
bool is_valid(const Family& f, const double* p);

}  // namespace sv
