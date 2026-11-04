#include "statviz/gpu.hpp"

#include <cmath>
#include <numbers>

namespace sv {

void pack_family(const Family& f, const double* p, float* texel) {
    double t[4] = {0, 0, 0, 0};
    switch (f.id) {
        case FamilyId::Normal: t[0] = p[0]; t[1] = 0.5 * p[1] * p[1]; break;
        case FamilyId::Exponential: t[0] = 1.0 / p[0]; break;
        case FamilyId::Gamma: t[0] = p[0]; t[1] = p[1]; break;
        case FamilyId::ChiSquared: t[0] = 0.5 * p[0]; t[1] = 2.0; break;
        case FamilyId::Uniform: t[0] = 0.5 * (p[0] + p[1]); t[1] = 0.5 * (p[1] - p[0]); break;
        case FamilyId::Laplace: t[0] = p[0]; t[1] = p[1] * p[1]; break;
        case FamilyId::Cauchy: t[0] = p[0]; t[1] = p[1]; break;
        case FamilyId::Logistic: t[0] = p[0]; t[1] = std::numbers::pi * p[1]; break;
        case FamilyId::Stable: {
            double T = p[0] == 1.0 ? 2.0 / std::numbers::pi : std::tan(0.5 * std::numbers::pi * p[0]);
            t[0] = 1.0 - p[0]; t[1] = p[1] * T; t[2] = p[2]; t[3] = p[3];
            break;
        }
        case FamilyId::Bernoulli: t[0] = p[0]; break;
        case FamilyId::Binomial: t[0] = p[0]; t[1] = p[1]; break;
        case FamilyId::Poisson: t[0] = p[0]; break;
        case FamilyId::Geometric: t[0] = p[0]; break;
        case FamilyId::NegBinomial: t[0] = p[0]; t[1] = p[1]; break;
        case FamilyId::DiscreteUniform: t[0] = 0.5 * (p[0] + p[1]); t[1] = p[1] - p[0] + 1.0; break;
    }
    for (int i = 0; i < 4; ++i) texel[i] = static_cast<float>(t[i]);
}

}  // namespace sv
