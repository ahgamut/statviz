#include <cmath>
#include <vector>

#include "doctest.h"
#include "statviz/scrub.hpp"

using sv::FamilyId;
using sv::Interpolated;
using sv::Scene;
using sv::ScrubHistory;
using sv::State;
using sv::TRange;

namespace {

// Normal(mu = u, sigma = 0.3 v)
Scene normal_scene() {
    Scene s;
    s.set_family(static_cast<int>(FamilyId::Normal));
    sv::expr::Error e;
    REQUIRE(s.set_param(0, "u", e));
    REQUIRE(s.set_param(1, "0.3*v", e));
    return s;
}

State uv(double u, double v) {
    State s{};
    s[sv::expr::U] = u;
    s[sv::expr::V] = v;
    return s;
}

const TRange kT{0, 1, 500};

void check_band(const Scene& sc, const State& start, const Interpolated& out, double step) {
    State prev = start;
    for (const State& s : out.states) {
        CHECK(sv::curve_distance(sc, prev, s, kT) <= step + 1e-12);
        prev = s;
    }
}

}  // namespace

TEST_CASE("curve_distance") {
    Scene sc = normal_scene();
    CHECK(sv::curve_distance(sc, uv(0.4, 1), uv(0.4, 1), kT) == 0.0);
    // mu 0 -> 1, sigma 0: max over t in [0,1] of |e^{it} - 1| = 2 sin(1/2)
    CHECK(sv::curve_distance(sc, uv(0, 0), uv(1, 0), kT) == doctest::Approx(2 * std::sin(0.5)));
    // Invalid (sigma < 0) compares as 0, so it is never subdivided.
    CHECK(sv::curve_distance(sc, uv(0, 0), uv(1, -1), kT) == 0.0);
}

TEST_CASE("push fills the gap between keys") {
    Scene sc = normal_scene();
    ScrubHistory h;
    Interpolated out;
    const double step = 0.01;

    h.push(sc, uv(0, 1), kT, step, out);
    REQUIRE(out.states.size() == 1);
    CHECK(out.states[0] == uv(0, 1));
    CHECK(out.keys[0] == 0);

    h.push(sc, uv(1, 1), kT, step, out);
    CHECK(out.states.size() > 50);
    CHECK(out.states.back() == uv(1, 1));
    for (int k : out.keys) CHECK(k == 1);
    check_band(sc, uv(0, 1), out, step);
    for (size_t i = 1; i < out.states.size(); ++i)
        CHECK(out.states[i][sv::expr::U] > out.states[i - 1][sv::expr::U]);

    h.push(sc, uv(1, 1), kT, step, out);  // no movement: just the key
    CHECK(out.states.size() == 1);
    CHECK(h.size() == 3);
}

TEST_CASE("bisection depth is capped") {
    Scene sc = normal_scene();
    ScrubHistory h;
    Interpolated out;
    h.push(sc, uv(0, 0), kT, 1e-12, out);
    h.push(sc, uv(1, 0), kT, 1e-12, out);
    CHECK(out.states.size() == (1u << sv::kMaxBisection));
}

TEST_CASE("invalid states are not interpolated") {
    Scene sc = normal_scene();
    ScrubHistory h;
    Interpolated out;
    h.push(sc, uv(0, 1), kT, 0.01, out);
    h.push(sc, uv(1, -1), kT, 0.01, out);
    CHECK(out.states.size() == 1);
}

TEST_CASE("replay at a finer step keeps the keys and refines the band") {
    Scene sc = normal_scene();
    ScrubHistory h;
    Interpolated out;
    for (double u : {0.0, 0.3, 0.35, 1.0}) h.push(sc, uv(u, 1), kT, 0.02, out);

    Interpolated coarse, fine;
    h.replay(sc, 0, h.size(), kT, 0.02, coarse);
    h.replay(sc, 0, h.size(), kT, 0.002, fine);
    CHECK(fine.states.size() > 5 * coarse.states.size());
    CHECK(fine.states.front() == uv(0, 1));
    CHECK(fine.states.back() == uv(1, 1));
    check_band(sc, uv(0, 1), fine, 0.002);
    for (size_t i = 1; i < fine.keys.size(); ++i) CHECK(fine.keys[i] >= fine.keys[i - 1]);

    // Replaying in batches gives the same states.
    Interpolated batched;
    h.replay(sc, 0, 2, kT, 0.002, batched);
    h.replay(sc, 2, 99, kT, 0.002, batched);
    CHECK(batched.states == fine.states);
    CHECK(batched.keys == fine.keys);
}
