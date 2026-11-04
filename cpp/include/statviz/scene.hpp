#pragma once

#include <array>
#include <string_view>

#include "statviz/expr.hpp"
#include "statviz/families.hpp"
#include "statviz/gpu.hpp"

// A family, one expression per parameter, and the values they are
// evaluated at: a u x v grid, or an explicit list of states.
namespace sv {

using State = std::array<double, expr::kNumVars>;  // u v a b c d R

struct Range {
    double lo = 0.0, hi = 1.0;
    int n = 1;
    double at(int i) const { return n > 1 ? lo + (hi - lo) * i / (n - 1) : lo; }
    double norm(double x) const { return hi != lo ? (x - lo) / (hi - lo) : 0.0; }
};

struct BuildStats {
    int curves = 0;
    int valid = 0;
    int bad[kMaxParams] = {};  // curves whose param i was out of domain
    void add(const BuildStats& o);
};

class Scene {
public:
    Scene();

    // Resets every parameter expression to the family's default value.
    bool set_family(int id);
    const Family& family() const { return *family_; }

    // On error the previous expression stays in effect.
    bool set_param(int i, std::string_view src, expr::Error& err);
    unsigned vars_used() const;  // over all parameter expressions

    // a-d and R; u and v come from the sweep (grid) or the state (list).
    void set_var(int var, double value) { base_[var] = value; }
    const State& base() const { return base_; }

    void set_sweep(int axis, Range r) { (axis == 0 ? u_ : v_) = r; }
    const Range& sweep(int axis) const { return axis == 0 ? u_ : v_; }
    void set_policy(InvalidPolicy p) { policy_ = p; }

    int grid_size() const { return u_.n * v_.n; }
    State grid_state(int curve) const;  // curve = j * u.n + i

    // Evaluates and validates the family parameters for s. Returns true if
    // the curve can be drawn; *bad gets the out-of-domain bitmask.
    bool eval(const State& s, double* p, unsigned* bad = nullptr) const;

    // Writes kCurveFloats floats per curve to out; params (optional) gets
    // kMaxParams doubles per curve.
    BuildStats build_grid(int first, int count, float* out, double* params = nullptr) const;
    BuildStats build_list(const State* states, int count, float* out, double* params = nullptr) const;

private:
    void write(const State& s, float* out, double* params, BuildStats& st) const;

    const Family* family_;
    std::array<expr::Program, kMaxParams> progs_;
    State base_{};
    Range u_, v_;
    InvalidPolicy policy_ = InvalidPolicy::Skip;
};

}  // namespace sv
