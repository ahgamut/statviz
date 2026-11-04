#include "statviz/scrub.hpp"

#include <algorithm>
#include <cmath>

namespace sv {

namespace {

using cf::cplx;

// phi at the distance samples; empty if the state is invalid.
std::vector<cplx> sample(const Scene& sc, const State& s, const TRange& ts) {
    double p[kMaxParams];
    if (!sc.eval(s, p)) return {};
    TRange d{ts.t0, ts.t1, std::min(ts.m, kDistanceSamples)};
    std::vector<cplx> z(d.m);
    for (int i = 0; i < d.m; ++i) z[i] = sc.family().phi(p, d.at(i));
    return z;
}

double distance(const std::vector<cplx>& a, const std::vector<cplx>& b) {
    if (a.empty() || b.empty()) return 0.0;
    double d = 0.0;
    for (size_t i = 0; i < a.size(); ++i) d = std::max(d, std::abs(a[i] - b[i]));
    return d;
}

State midpoint(const State& a, const State& b) {
    State m;
    for (size_t i = 0; i < m.size(); ++i) m[i] = 0.5 * (a[i] + b[i]);
    return m;
}

struct Bisector {
    const Scene& sc;
    const TRange& ts;
    double max_step;
    int key;
    Interpolated& out;

    // Emits the states after a, up to and including b.
    void run(const State& a, const std::vector<cplx>& za, const State& b, const std::vector<cplx>& zb, int depth) {
        if (depth < kMaxBisection && distance(za, zb) > max_step) {
            State m = midpoint(a, b);
            std::vector<cplx> zm = sample(sc, m, ts);
            if (!zm.empty()) {
                run(a, za, m, zm, depth + 1);
                run(m, zm, b, zb, depth + 1);
                return;
            }
        }
        out.states.push_back(b);
        out.keys.push_back(key);
    }
};

void segment(const Scene& sc, const State& a, const State& b, int key, const TRange& ts, double max_step,
             Interpolated& out) {
    Bisector{sc, ts, max_step, key, out}.run(a, sample(sc, a, ts), b, sample(sc, b, ts), 0);
}

}  // namespace

double curve_distance(const Scene& sc, const State& a, const State& b, const TRange& ts) {
    return distance(sample(sc, a, ts), sample(sc, b, ts));
}

void ScrubHistory::push(const Scene& sc, const State& key, const TRange& ts, double max_step, Interpolated& out) {
    out.clear();
    int k = size();
    if (k == 0) {
        out.states.push_back(key);
        out.keys.push_back(0);
    } else {
        segment(sc, keys_.back(), key, k, ts, max_step, out);
    }
    keys_.push_back(key);
}

int ScrubHistory::replay(const Scene& sc, int k0, int k1, const TRange& ts, double max_step, Interpolated& out,
                         size_t max_states) const {
    k0 = std::max(k0, 0);
    k1 = std::min(k1, size());
    for (int k = k0; k < k1; ++k) {
        if (k > k0 && out.states.size() >= max_states) return k;
        if (k == 0) {
            out.states.push_back(keys_[0]);
            out.keys.push_back(0);
        } else {
            segment(sc, keys_[k - 1], keys_[k], k, ts, max_step, out);
        }
    }
    return std::max(k0, k1);
}

}  // namespace sv
