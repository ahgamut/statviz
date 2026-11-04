#pragma once

#include <vector>

#include "statviz/scene.hpp"

// Scrub mode: slider states pushed while dragging become keys; between
// consecutive keys, states are inserted until neighbouring curves are at
// most max_step apart (in unit-disk units), so a fast drag still sweeps a
// smooth band. The keys are kept so the band can be re-interpolated at
// export resolution.
namespace sv {

struct TRange {
    double t0 = 0.0, t1 = 1.0;
    int m = 2;
    double at(int i) const { return m > 1 ? t0 + (t1 - t0) * i / (m - 1) : t0; }
};

inline constexpr int kDistanceSamples = 256;  // t samples used to compare curves
inline constexpr int kMaxBisection = 12;      // at most 2^12 states between two keys

// max over sampled t of |phi(t; a) - phi(t; b)|; 0 if either is invalid.
double curve_distance(const Scene& sc, const State& a, const State& b, const TRange& ts);

struct Interpolated {
    std::vector<State> states;
    std::vector<int> keys;  // index of the key each state leads up to
    void clear() { states.clear(); keys.clear(); }
};

class ScrubHistory {
public:
    void clear() { keys_.clear(); }
    int size() const { return static_cast<int>(keys_.size()); }
    const std::vector<State>& keys() const { return keys_; }

    // Appends key; out gets the states to draw, ending with key itself.
    void push(const Scene& sc, const State& key, const TRange& ts, double max_step, Interpolated& out);

    // Re-interpolates keys [k0, k1) (k1 clamped to size()); out is appended to.
    void replay(const Scene& sc, int k0, int k1, const TRange& ts, double max_step, Interpolated& out) const;

private:
    std::vector<State> keys_;
};

}  // namespace sv
