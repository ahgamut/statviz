#include "statviz/points.hpp"

#include <algorithm>

namespace sv {

void generate_points(const Scene& sc, const State* states, int count, const TRange& ts, float* out) {
    const Family& f = sc.family();
    for (int c = 0; c < count; ++c) {
        float* o = out + static_cast<size_t>(c) * ts.m * 2;
        double p[kMaxParams];
        if (!sc.eval(states[c], p)) {
            std::fill(o, o + ts.m * 2, 0.0f);
            continue;
        }
        for (int i = 0; i < ts.m; ++i) {
            cf::cplx z = f.phi(p, ts.at(i));
            o[2 * i] = static_cast<float>(z.real());
            o[2 * i + 1] = static_cast<float>(z.imag());
        }
    }
}

}  // namespace sv
