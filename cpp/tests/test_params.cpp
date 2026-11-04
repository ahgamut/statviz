#include <vector>

#include "doctest.h"
#include "statviz/params.hpp"

TEST_CASE("normal_texel layout") {
    float t[sv::kTexelFloats];
    sv::normal_texel(2.0, 3.0, t);
    CHECK(t[0] == 2.0f);
    CHECK(t[1] == 4.5f);
    CHECK(t[2] == 0.0f);
    CHECK(t[3] == 0.0f);
}

TEST_CASE("normal_sweep spans [s0, s1]") {
    const int n = 5;
    std::vector<float> out(n * sv::kTexelFloats);
    sv::normal_sweep(1.0, 0.0, 2.0, n, out.data());
    for (int i = 0; i < n; ++i) {
        double s = 2.0 * i / (n - 1);
        CHECK(out[i * sv::kTexelFloats] == 1.0f);
        CHECK(out[i * sv::kTexelFloats + 1] == doctest::Approx(0.5 * s * s));
    }

    float one[sv::kTexelFloats];
    sv::normal_sweep(0.0, 0.7, 9.0, 1, one);
    CHECK(one[1] == doctest::Approx(0.5 * 0.7 * 0.7));
}
