#include <vector>

#include "doctest.h"
#include "statviz/points.hpp"

TEST_CASE("generate_points evaluates phi per curve and t sample") {
    sv::Scene s;
    s.set_family(static_cast<int>(sv::FamilyId::Poisson));
    sv::expr::Error e;
    REQUIRE(s.set_param(0, "10*u - 2", e));  // negative (invalid) for u < 0.2
    s.set_sweep(0, {0, 1, 6});

    std::vector<sv::State> states;
    for (int c = 0; c < s.grid_size(); ++c) states.push_back(s.grid_state(c));
    const sv::TRange ts{-3, 9, 7};
    std::vector<float> out(states.size() * ts.m * 2, -1.0f);
    sv::generate_points(s, states.data(), static_cast<int>(states.size()), ts, out.data());

    for (size_t c = 0; c < states.size(); ++c) {
        CAPTURE(c);
        double lambda = 10 * states[c][sv::expr::U] - 2;
        for (int i = 0; i < ts.m; ++i) {
            const float* xy = &out[(c * ts.m + i) * 2];
            if (lambda < 0) {
                CHECK(xy[0] == 0.0f);
                CHECK(xy[1] == 0.0f);
            } else {
                sv::cf::cplx z = sv::cf::poisson(ts.at(i), lambda);
                CHECK(xy[0] == static_cast<float>(z.real()));
                CHECK(xy[1] == static_cast<float>(z.imag()));
            }
        }
    }
}
