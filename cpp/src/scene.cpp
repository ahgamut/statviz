#include "statviz/scene.hpp"

#include <cstdio>

namespace sv {

void BuildStats::add(const BuildStats& o) {
    curves += o.curves;
    valid += o.valid;
    for (int i = 0; i < kMaxParams; ++i) bad[i] += o.bad[i];
}

Scene::Scene() { set_family(0); }

bool Scene::set_family(int id) {
    const Family* f = find_family(id);
    if (!f) return false;
    family_ = f;
    for (int i = 0; i < kMaxParams; ++i) {
        progs_[i] = {};
        if (i >= f->nparams) continue;
        char buf[32];
        std::snprintf(buf, sizeof buf, "%.17g", f->params[i].fallback);
        expr::Error err;
        expr::compile(buf, progs_[i], err);
    }
    return true;
}

bool Scene::set_param(int i, std::string_view src, expr::Error& err) {
    if (i < 0 || i >= family_->nparams) {
        err = {0, 0, "no such parameter"};
        return false;
    }
    expr::Program p;
    if (!expr::compile(src, p, err)) return false;
    progs_[i] = std::move(p);
    return true;
}

unsigned Scene::vars_used() const {
    unsigned m = 0;
    for (int i = 0; i < family_->nparams; ++i) m |= progs_[i].vars_used();
    return m;
}

State Scene::grid_state(int curve) const {
    State s = base_;
    s[expr::U] = u_.at(curve % u_.n);
    s[expr::V] = v_.at(curve / u_.n);
    return s;
}

bool Scene::eval(const State& s, double* p, unsigned* bad) const {
    for (int i = 0; i < family_->nparams; ++i) p[i] = progs_[i].eval(s.data());
    for (int i = family_->nparams; i < kMaxParams; ++i) p[i] = 0.0;
    unsigned b = validate(*family_, p, policy_);
    if (bad) *bad = b;
    return is_valid(*family_, p);
}

void Scene::write(const State& s, float* out, double* params, BuildStats& st) const {
    double p[kMaxParams];
    unsigned bad = 0;
    bool ok = eval(s, p, &bad);
    ++st.curves;
    if (ok) ++st.valid;
    for (int i = 0; i < family_->nparams; ++i)
        if (bad & (1u << i)) ++st.bad[i];

    float* t0 = out;
    float* t1 = out + 4;
    if (ok) {
        pack_family(*family_, p, t0);
    } else {
        for (int i = 0; i < 4; ++i) t0[i] = 0.0f;
    }
    t1[0] = static_cast<float>(u_.norm(s[expr::U]));
    t1[1] = static_cast<float>(v_.norm(s[expr::V]));
    t1[2] = ok ? 1.0f : 0.0f;
    t1[3] = 0.0f;
    if (params)
        for (int i = 0; i < kMaxParams; ++i) params[i] = p[i];
}

BuildStats Scene::build_grid(int first, int count, float* out, double* params) const {
    BuildStats st;
    for (int k = 0; k < count; ++k)
        write(grid_state(first + k), out + k * kCurveFloats, params ? params + k * kMaxParams : nullptr, st);
    return st;
}

BuildStats Scene::build_list(const State* states, int count, float* out, double* params) const {
    BuildStats st;
    for (int k = 0; k < count; ++k)
        write(states[k], out + k * kCurveFloats, params ? params + k * kMaxParams : nullptr, st);
    return st;
}

}  // namespace sv
