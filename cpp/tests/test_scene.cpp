#include <cmath>
#include <numbers>
#include <string>
#include <vector>

#include "doctest.h"
#include "statviz/meta.hpp"
#include "statviz/scene.hpp"

using sv::FamilyId;
using sv::Scene;
using sv::State;
using sv::expr::Error;

namespace {

int id(FamilyId f) { return static_cast<int>(f); }

void set(Scene& s, int i, const char* src) {
    Error e;
    CAPTURE(src);
    REQUIRE(s.set_param(i, src, e));
}

}  // namespace

TEST_CASE("grid states sweep u fastest") {
    Scene s;
    s.set_sweep(0, {0, 1, 3});
    s.set_sweep(1, {10, 20, 2});
    s.set_var(sv::expr::A, 7);
    REQUIRE(s.grid_size() == 6);
    State st = s.grid_state(4);  // i = 1, j = 1
    CHECK(st[sv::expr::U] == 0.5);
    CHECK(st[sv::expr::V] == 20);
    CHECK(st[sv::expr::A] == 7);
    CHECK(s.grid_state(0)[sv::expr::U] == 0);
    CHECK(s.grid_state(2)[sv::expr::U] == 1);

    s.set_sweep(0, {0.3, 0.9, 1});  // one sample sits at lo
    CHECK(s.grid_state(0)[sv::expr::U] == 0.3);
}

TEST_CASE("Normal with mu = 2uv, sigma = (u+v)^2") {
    Scene s;
    REQUIRE(s.set_family(id(FamilyId::Normal)));
    set(s, 0, "2*u*v");
    set(s, 1, "(u+v)^2");
    CHECK(s.vars_used() == ((1u << sv::expr::U) | (1u << sv::expr::V)));
    s.set_sweep(0, {0, 1, 5});
    s.set_sweep(1, {0, 1, 4});

    const int n = s.grid_size();
    std::vector<float> out(n * sv::kCurveFloats);
    std::vector<double> params(n * sv::kMaxParams);
    sv::BuildStats st = s.build_grid(0, n, out.data(), params.data());
    CHECK(st.curves == 20);
    CHECK(st.valid == 20);

    for (int c = 0; c < n; ++c) {
        double u = (c % 5) / 4.0, v = (c / 5) / 3.0;
        CAPTURE(c);
        CHECK(params[c * sv::kMaxParams] == doctest::Approx(2 * u * v));
        CHECK(params[c * sv::kMaxParams + 1] == doctest::Approx((u + v) * (u + v)));
        const float* t = &out[c * sv::kCurveFloats];
        CHECK(t[0] == doctest::Approx(2 * u * v));
        CHECK(t[1] == doctest::Approx(0.5 * std::pow(u + v, 4)));
        CHECK(t[4] == doctest::Approx(u));
        CHECK(t[5] == doctest::Approx(v));
        CHECK(t[6] == 1.0f);
    }

    // A slab matches the same curves of the full build.
    std::vector<float> slab(3 * sv::kCurveFloats);
    s.build_grid(7, 3, slab.data());
    for (int k = 0; k < 3 * sv::kCurveFloats; ++k) CHECK(slab[k] == out[7 * sv::kCurveFloats + k]);
}

TEST_CASE("mu = u, sigma = u^2 depends on u only") {
    Scene s;
    s.set_family(id(FamilyId::Normal));
    set(s, 0, "u");
    set(s, 1, "u^2");
    CHECK(s.vars_used() == (1u << sv::expr::U));
}

TEST_CASE("out-of-domain parameters: skip or clamp") {
    Scene s;
    s.set_family(id(FamilyId::Normal));
    set(s, 0, "0");
    set(s, 1, "u - 0.5");  // negative for u < 0.5
    s.set_sweep(0, {0, 1, 11});
    std::vector<float> out(11 * sv::kCurveFloats);

    sv::BuildStats skip = s.build_grid(0, 11, out.data());
    CHECK(skip.valid == 6);
    CHECK(skip.bad[0] == 0);
    CHECK(skip.bad[1] == 5);
    for (int c = 0; c < 11; ++c) CHECK(out[c * sv::kCurveFloats + 6] == (c >= 5 ? 1.0f : 0.0f));
    CHECK(out[0] == 0.0f);  // invalid curves get zeroed constants

    s.set_policy(sv::InvalidPolicy::Clamp);
    sv::BuildStats clamp = s.build_grid(0, 11, out.data());
    CHECK(clamp.valid == 11);
    CHECK(clamp.bad[1] == 5);
    CHECK(out[1] == 0.0f);  // sigma clamped to 0

    set(s, 1, "log(u - 2)");  // NaN everywhere: invalid even when clamping
    sv::BuildStats nan = s.build_grid(0, 11, out.data());
    CHECK(nan.valid == 0);
    CHECK(nan.bad[1] == 11);
}

TEST_CASE("a bad expression keeps the previous one") {
    Scene s;
    s.set_family(id(FamilyId::Poisson));
    set(s, 0, "3 + u");
    Error e;
    CHECK_FALSE(s.set_param(0, "3 +", e));
    CHECK(e.pos == 3);
    CHECK_FALSE(s.set_param(1, "1", e));  // Poisson has one parameter
    double p[sv::kMaxParams];
    State st{};
    st[sv::expr::U] = 2;
    CHECK(s.eval(st, p));
    CHECK(p[0] == 5);
}

TEST_CASE("set_family resets parameters to defaults") {
    Scene s;
    for (const sv::Family& f : sv::families()) {
        CAPTURE(f.key);
        REQUIRE(s.set_family(static_cast<int>(f.id)));
        double p[sv::kMaxParams];
        CHECK(s.eval(State{}, p));
        for (int i = 0; i < f.nparams; ++i) CHECK(p[i] == f.params[i].fallback);
        CHECK(s.vars_used() == 0);
    }
    CHECK_FALSE(s.set_family(-1));
    CHECK_FALSE(s.set_family(99));
}

TEST_CASE("pack_family writes the documented layouts") {
    float t[4];
    auto pack = [&](FamilyId f, std::vector<double> p) { sv::pack_family(*sv::find_family(id(f)), p.data(), t); };
    pack(FamilyId::Exponential, {4});
    CHECK(t[0] == 0.25f);
    pack(FamilyId::ChiSquared, {6});
    CHECK(t[0] == 3.0f);
    CHECK(t[1] == 2.0f);
    pack(FamilyId::Uniform, {-1, 3});
    CHECK(t[0] == 1.0f);
    CHECK(t[1] == 2.0f);
    pack(FamilyId::Logistic, {0.5, 2});
    CHECK(t[1] == doctest::Approx(2 * std::numbers::pi));
    pack(FamilyId::Stable, {1.5, 0.5, 2, -1});
    CHECK(t[1] == doctest::Approx(0.5 * std::tan(0.75 * std::numbers::pi)));
    CHECK(t[2] == 2.0f);
    CHECK(t[3] == -1.0f);
    pack(FamilyId::Stable, {1, 0.5, 2, -1});
    CHECK(t[1] == doctest::Approx(0.5 * 2 / std::numbers::pi));
    pack(FamilyId::DiscreteUniform, {-2, 4});
    CHECK(t[0] == 1.0f);
    CHECK(t[1] == 7.0f);
}

TEST_CASE("build_list evaluates explicit states") {
    Scene s;
    s.set_family(id(FamilyId::Normal));
    set(s, 0, "a + R");
    set(s, 1, "v");
    s.set_sweep(0, {0, 2, 3});
    State a{}, b{};
    a[sv::expr::A] = 1;
    a[sv::expr::U] = 1;
    a[sv::expr::V] = 0.5;
    b[sv::expr::R] = 0.25;
    b[sv::expr::V] = -1;  // sigma < 0: skipped
    State states[] = {a, b};
    float out[2 * sv::kCurveFloats];
    sv::BuildStats st = s.build_list(states, 2, out);
    CHECK(st.valid == 1);
    CHECK(out[0] == 1.0f);
    CHECK(out[4] == 0.5f);  // u = 1 normalized over [0, 2]
    CHECK(out[6] == 1.0f);
    CHECK(out[sv::kCurveFloats + 6] == 0.0f);
}

TEST_CASE("metadata JSON") {
    std::string j = sv::metadata_json();
    CHECK(j.front() == '{');
    CHECK(j.back() == '}');
    CHECK(j.find("\"key\":\"normal\"") != std::string::npos);
    CHECK(j.find("\"symbol\":\"σ\"") != std::string::npos);
    CHECK(j.find("\\\\varphi") != std::string::npos);  // backslashes escaped
    CHECK(j.find("\"vars\":[\"u\",\"v\",\"a\",\"b\",\"c\",\"d\",\"R\"]") != std::string::npos);
    CHECK(j.find("\"curveFloats\":8") != std::string::npos);
    int depth = 0;
    bool in_str = false;
    for (size_t i = 0; i < j.size(); ++i) {
        char c = j[i];
        if (in_str) {
            if (c == '\\') ++i;
            else if (c == '"') in_str = false;
        } else if (c == '"') in_str = true;
        else if (c == '{' || c == '[') ++depth;
        else if (c == '}' || c == ']') --depth;
        CHECK(depth >= 0);
    }
    CHECK(depth == 0);
}
