// C ABI exported to JS. Strings go in via sv_scratch (UTF-8, not
// NUL-terminated); returned pointers stay valid until the next call that
// writes the same buffer.
#include <emscripten/emscripten.h>

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

#include "statviz/meta.hpp"
#include "statviz/png.hpp"
#include "statviz/points.hpp"
#include "statviz/scene.hpp"
#include "statviz/scrub.hpp"

#define SV_API extern "C" EMSCRIPTEN_KEEPALIVE

namespace {
sv::Scene g_scene;
std::vector<char> g_scratch(256);
sv::expr::Error g_error;
std::vector<float> g_curves;
int g_stats[2 + sv::kMaxParams];
std::string g_meta;
std::vector<float> g_points;
std::vector<sv::State> g_grid_states;
constexpr long kMaxPoints = 1 << 21;  // 16 MB of (Re, Im) floats per call
constexpr int kMaxCurves = 1 << 19;   // 16 MB of curve texels per call
constexpr int kStateChunk = 4096;
int g_replay_next = 0;
double g_state[sv::expr::kNumVars];
sv::ScrubHistory g_history;
sv::Interpolated g_interp;
sv::PngWriter g_png;
std::vector<uint8_t> g_png_in;
int g_png_w = 0;

void set_stats(const sv::BuildStats& st) {
    g_stats[0] = st.curves;
    g_stats[1] = st.valid;
    for (int i = 0; i < sv::kMaxParams; ++i) g_stats[2 + i] = st.bad[i];
}

int build_interp() {
    int n = static_cast<int>(g_interp.states.size());
    g_curves.resize(static_cast<size_t>(std::max(n, 1)) * sv::kCurveFloats);
    set_stats(g_scene.build_list(g_interp.states.data(), n, g_curves.data()));
    return n;
}
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

SV_API int sv_max_curves() { return kMaxCurves; }

// Builds grid curves [first, first + count), at most sv_max_curves(); the
// number built is in sv_stats()[0]. Returns kCurveFloats floats per curve.
SV_API float* sv_build_grid(int first, int count) {
    int n = g_scene.grid_size();
    first = std::clamp(first, 0, n);
    count = std::clamp(count, 0, std::min(n - first, kMaxCurves));
    g_curves.resize(static_cast<size_t>(std::max(count, 1)) * sv::kCurveFloats);
    set_stats(g_scene.build_grid(first, count, g_curves.data()));
    return g_curves.data();
}

// Curve data of the last build (grid, scrub push or replay).
SV_API float* sv_curves() { return g_curves.data(); }

// Stats of the last build: curves, valid, then out-of-domain counts per param.
SV_API const int* sv_stats() { return g_stats; }

// JS writes a state (u v a b c d R) here before sv_scrub_push.
SV_API double* sv_state() { return g_state; }

SV_API void sv_scrub_clear() { g_history.clear(); }
SV_API int sv_scrub_size() { return g_history.size(); }

// Appends the state as a key and builds the curves leading to it, at most
// max_step apart in unit-disk units. Returns the number of curves.
SV_API int sv_scrub_push(double t0, double t1, int m, double max_step) {
    sv::State key;
    std::copy(g_state, g_state + sv::expr::kNumVars, key.begin());
    g_history.push(g_scene, key, {t0, t1, m}, max_step, g_interp);
    return build_interp();
}

// Rebuilds the curves for keys [k0, k1) at a new max_step, stopping at a
// key boundary near sv_max_curves(); continue from sv_scrub_replay_next().
SV_API int sv_scrub_replay(int k0, int k1, double t0, double t1, int m, double max_step) {
    g_interp.clear();
    g_replay_next = g_history.replay(g_scene, k0, k1, {t0, t1, m}, max_step, g_interp, kMaxCurves - (1 << sv::kMaxBisection));
    return build_interp();
}

SV_API int sv_scrub_replay_next() { return g_replay_next; }

// Key index of each curve from the last push or replay.
SV_API const int* sv_scrub_curve_keys() { return g_interp.keys.data(); }

SV_API int sv_max_points() { return static_cast<int>(kMaxPoints); }

// CPU path: (Re, Im) of grid curves [first, first + count) at m samples of
// [t0, t1], curve-major. nullptr if count * m exceeds sv_max_points().
SV_API float* sv_points_grid(int first, int count, double t0, double t1, int m) {
    int n = g_scene.grid_size();
    first = std::clamp(first, 0, n);
    count = std::clamp(count, 0, n - first);
    if (m < 1 || static_cast<long>(count) * m > kMaxPoints) return nullptr;
    g_points.resize(static_cast<size_t>(std::max(count * m, 1)) * 2);
    for (int c0 = 0; c0 < count; c0 += kStateChunk) {
        int k = std::min(kStateChunk, count - c0);
        g_grid_states.resize(k);
        for (int i = 0; i < k; ++i) g_grid_states[i] = g_scene.grid_state(first + c0 + i);
        sv::generate_points(g_scene, g_grid_states.data(), k, {t0, t1, m},
                            g_points.data() + static_cast<size_t>(c0) * m * 2);
    }
    return g_points.data();
}

// Same for the curves of the last scrub push or replay.
SV_API float* sv_points_scrub(double t0, double t1, int m) {
    int count = static_cast<int>(g_interp.states.size());
    if (m < 1 || static_cast<long>(count) * m > kMaxPoints) return nullptr;
    g_points.resize(static_cast<size_t>(std::max(count * m, 1)) * 2);
    sv::generate_points(g_scene, g_interp.states.data(), count, {t0, t1, m}, g_points.data());
    return g_points.data();
}

// PNG export. Each call returns the number of encoded bytes now at
// sv_png_out() (valid until the next sv_png_* call), or -1 on error.
SV_API int sv_png_begin(int w, int h, int alpha) {
    g_png_w = w;
    return g_png.begin(w, h, alpha != 0) ? static_cast<int>(g_png.out().size()) : -1;
}

// tEXt chunk "keyword\0text" from the first len bytes of the scratch buffer.
SV_API int sv_png_text(int len) {
    std::string_view s(g_scratch.data(), std::clamp(len, 0, static_cast<int>(g_scratch.size())));
    size_t nul = s.find('\0');
    if (nul == std::string_view::npos) return -1;
    return g_png.text(s.substr(0, nul), s.substr(nul + 1)) ? static_cast<int>(g_png.out().size()) : -1;
}

// Where JS writes n RGBA rows before sv_png_rows(n).
SV_API uint8_t* sv_png_input(int n) {
    g_png_in.resize(static_cast<size_t>(std::max(n, 1)) * std::max(g_png_w, 1) * 4);
    return g_png_in.data();
}

SV_API int sv_png_rows(int n) {
    if (static_cast<size_t>(std::max(n, 0)) * g_png_w * 4 > g_png_in.size()) return -1;
    return g_png.rows(g_png_in.data(), n) ? static_cast<int>(g_png.out().size()) : -1;
}

SV_API int sv_png_end() {
    bool ok = g_png.end();
    g_png_in = {};
    return ok ? static_cast<int>(g_png.out().size()) : -1;
}

SV_API const uint8_t* sv_png_out() { return g_png.out().data(); }
