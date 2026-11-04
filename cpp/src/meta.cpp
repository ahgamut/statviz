#include "statviz/meta.hpp"

#include <cstdio>

#include "statviz/expr.hpp"
#include "statviz/families.hpp"
#include "statviz/gpu.hpp"

namespace sv {

namespace {

const char* domain_key(Domain d) {
    switch (d) {
        case Domain::Real: return "real";
        case Domain::NonNegative: return "nonneg";
        case Domain::Positive: return "pos";
        case Domain::Unit: return "unit";
        case Domain::UnitPositive: return "unit_pos";
        case Domain::Count: return "count";
        case Domain::Integer: return "int";
        case Domain::StableAlpha: return "stable_alpha";
        case Domain::Skew: return "skew";
    }
    return "?";
}

void str(std::string& o, const char* s) {
    o += '"';
    for (const char* c = s; *c; ++c) {
        unsigned char ch = static_cast<unsigned char>(*c);
        if (ch == '"' || ch == '\\') {
            o += '\\';
            o += *c;
        } else if (ch < 0x20) {
            char buf[8];
            std::snprintf(buf, sizeof buf, "\\u%04x", ch);
            o += buf;
        } else {
            o += *c;  // UTF-8 passes through
        }
    }
    o += '"';
}

void num(std::string& o, double x) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.17g", x);
    o += buf;
}

}  // namespace

std::string metadata_json() {
    std::string o = "{\"families\":[";
    bool first = true;
    for (const Family& f : families()) {
        if (!first) o += ',';
        first = false;
        o += "{\"id\":";
        num(o, static_cast<int>(f.id));
        o += ",\"key\":";
        str(o, f.key);
        o += ",\"name\":";
        str(o, f.name);
        o += ",\"lattice\":";
        o += f.lattice ? "true" : "false";
        o += ",\"latex\":";
        str(o, f.latex);
        o += ",\"params\":[";
        for (int i = 0; i < f.nparams; ++i) {
            const ParamSpec& p = f.params[i];
            if (i) o += ',';
            o += "{\"name\":";
            str(o, p.name);
            o += ",\"symbol\":";
            str(o, p.symbol);
            o += ",\"domain\":";
            str(o, domain_key(p.domain));
            o += ",\"domainText\":";
            str(o, domain_name(p.domain));
            o += ",\"default\":";
            num(o, p.fallback);
            o += '}';
        }
        o += "]}";
    }
    o += "],\"vars\":[";
    for (int i = 0; i < expr::kNumVars; ++i) {
        if (i) o += ',';
        str(o, expr::kVarNames[i]);
    }
    o += "],\"curveFloats\":";
    num(o, kCurveFloats);
    o += ",\"maxParams\":";
    num(o, kMaxParams);
    o += '}';
    return o;
}

}  // namespace sv
