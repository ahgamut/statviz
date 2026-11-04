// C ABI exported to JS. Strings go in via sv_scratch (UTF-8, not
// NUL-terminated); returned pointers stay valid until the next call that
// writes the same buffer.
#include <emscripten/emscripten.h>

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

#include "statviz/meta.hpp"
#include "statviz/scene.hpp"

#define SV_API extern "C" EMSCRIPTEN_KEEPALIVE

namespace {
sv::Scene g_scene;
std::vector<char> g_scratch(256);
sv::expr::Error g_error;
std::vector<float> g_curves;
int g_stats[2 + sv::kMaxParams];
std::string g_meta;
}  // namespace

SV_API char* sv_scratch(int nbytes) {
    if (nbytes + 1 > static_cast<int>(g_scratch.size())) g_scratch.resize(nbytes + 1);
    return g_scratch.data();
}

SV_API const char* sv_metadata() {
    if (g_meta.empty()) g_meta = sv::metadata_json();
    return g_meta.c_str();
}

SV_API int sv_set_family(int id) { return g_scene.set_family(id) ? 0 : -1; }

// Source is the first len bytes of the scratch buffer. 0 on success.
SV_API int sv_set_param(int index, int len) {
    len = std::clamp(len, 0, static_cast<int>(g_scratch.size()));
    return g_scene.set_param(index, std::string_view(g_scratch.data(), len), g_error) ? 0 : 1;
}

SV_API int sv_error_pos() { return g_error.pos; }
SV_API int sv_error_len() { return g_error.len; }
SV_API const char* sv_error_msg() { return g_error.message.c_str(); }

SV_API unsigned sv_vars_used() { return g_scene.vars_used(); }

SV_API void sv_set_var(int var, double value) {
    if (var >= 0 && var < sv::expr::kNumVars) g_scene.set_var(var, value);
}

SV_API void sv_set_sweep(int axis, double lo, double hi, int n) {
    g_scene.set_sweep(axis == 0 ? 0 : 1, {lo, hi, std::max(n, 1)});
}

SV_API void sv_set_policy(int clamp) {
    g_scene.set_policy(clamp ? sv::InvalidPolicy::Clamp : sv::InvalidPolicy::Skip);
}

SV_API int sv_grid_size() { return g_scene.grid_size(); }

// Builds grid curves [first, first + count); returns count * kCurveFloats floats.
SV_API float* sv_build_grid(int first, int count) {
    int n = g_scene.grid_size();
    first = std::clamp(first, 0, n);
    count = std::clamp(count, 0, n - first);
    g_curves.resize(static_cast<size_t>(std::max(count, 1)) * sv::kCurveFloats);
    sv::BuildStats st = g_scene.build_grid(first, count, g_curves.data());
    g_stats[0] = st.curves;
    g_stats[1] = st.valid;
    for (int i = 0; i < sv::kMaxParams; ++i) g_stats[2 + i] = st.bad[i];
    return g_curves.data();
}

// Stats of the last build: curves, valid, then out-of-domain counts per param.
SV_API const int* sv_stats() { return g_stats; }
