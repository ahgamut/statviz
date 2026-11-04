// C ABI exported to JS. Returned pointers stay valid until the next call
// that writes the same buffer.
#include <emscripten/emscripten.h>

#include <vector>

#include "statviz/params.hpp"

#define SV_API extern "C" EMSCRIPTEN_KEEPALIVE

namespace {
std::vector<float> g_params;
}

SV_API int sv_texel_floats() { return sv::kTexelFloats; }

SV_API float* sv_normal_sweep(double mu, double s0, double s1, int n) {
    if (n < 1) return nullptr;
    g_params.resize(static_cast<size_t>(n) * sv::kTexelFloats);
    sv::normal_sweep(mu, s0, s1, n, g_params.data());
    return g_params.data();
}
