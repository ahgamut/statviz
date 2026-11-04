#include <cmath>
#include <numbers>
#include <string>

#include "doctest.h"
#include "statviz/expr.hpp"

using namespace sv::expr;

namespace {

// u=3, v=0.5, a=-2, b=10, c=0, d=1, R=0.25
const double kVars[kNumVars] = {3, 0.5, -2, 10, 0, 1, 0.25};

double eval(const std::string& src) {
    Program p;
    Error e;
    bool ok = compile(src, p, e);
    CAPTURE(src);
    CAPTURE(e.message);
    REQUIRE(ok);
    return p.eval(kVars);
}

Error error_of(const std::string& src) {
    Program p;
    Error e;
    CAPTURE(src);
    REQUIRE_FALSE(compile(src, p, e));
    CHECK(p.empty());
    return e;
}

bool contains(const std::string& s, const std::string& part) { return s.find(part) != std::string::npos; }

}  // namespace

TEST_CASE("arithmetic and precedence") {
    CHECK(eval("1 + 2 * 3") == 7);
    CHECK(eval("(1 + 2) * 3") == 9);
    CHECK(eval("10 - 4 - 3") == 3);
    CHECK(eval("8 / 4 / 2") == 1);
    CHECK(eval("2 ^ 3 ^ 2") == 512);
    CHECK(eval("2 ** 3") == 8);
    CHECK(eval("-2^2") == -4);
    CHECK(eval("(-2)^2") == 4);
    CHECK(eval("2^-1") == 0.5);
    CHECK(eval("--3") == 3);
    CHECK(eval("+4") == 4);
    CHECK(eval("1 - -1") == 2);
    CHECK(eval("3e2") == 300);
    CHECK(eval("1.5e-3") == doctest::Approx(0.0015));
    CHECK(eval(".5 + 1.") == 1.5);
    CHECK(eval("1e-400") == 0);
    CHECK(std::isinf(eval("1e400")));
}

TEST_CASE("variables, constants and implicit multiplication") {
    CHECK(eval("u") == 3);
    CHECK(eval("R") == 0.25);
    CHECK(eval("2*u*v") == 3);
    CHECK(eval("(u+v)^2") == 12.25);
    CHECK(eval("u^2") == 9);
    CHECK(eval("2u") == 6);
    CHECK(eval("2u^2") == 18);
    CHECK(eval("3(u + v)") == 10.5);
    CHECK(eval("2pi") == doctest::Approx(2 * std::numbers::pi));
    CHECK(eval("2e") == doctest::Approx(2 * std::numbers::e));
    CHECK(eval("a*b + c - d") == -21);
}

TEST_CASE("functions") {
    CHECK(eval("sin(0)") == 0);
    CHECK(eval("cos(pi)") == doctest::Approx(-1));
    CHECK(eval("atan2(1, 1)") == doctest::Approx(std::numbers::pi / 4));
    CHECK(eval("clamp(5, 0, 1)") == 1);
    CHECK(eval("clamp(-5, 0, 1)") == 0);
    CHECK(eval("mod(-1, 3)") == 2);
    CHECK(eval("mod(7, 3)") == 1);
    CHECK(eval("min(u, v) + max(u, v)") == 3.5);
    CHECK(eval("sign(-3) + sign(0) + sign(2)") == 0);
    CHECK(eval("pow(2, 10)") == 1024);
    CHECK(eval("sqrt(16) + abs(-2) + floor(2.7) + ceil(2.1) + round(2.5)") == 4 + 2 + 2 + 3 + 3);
    CHECK(eval("exp(log(5))") == doctest::Approx(5));
    CHECK(eval("log2(8) + log10(1000)") == doctest::Approx(6));
    CHECK(eval("tanh(0) + sinh(0) + cosh(0)") == 1);
    CHECK(eval("sin(2u) + 0") == doctest::Approx(std::sin(6.0)));
    CHECK(std::isnan(eval("sqrt(-1)")));
    CHECK(std::isinf(eval("1 / c")));
}

TEST_CASE("vars_used reports referenced variables") {
    Program p;
    Error e;
    REQUIRE(compile("2*u*v + sin(a) + R", p, e));
    CHECK(p.vars_used() == ((1u << U) | (1u << V) | (1u << A) | (1u << R)));
    REQUIRE(compile("pi + 2", p, e));
    CHECK(p.vars_used() == 0);
}

TEST_CASE("errors carry a position and a message") {
    Error e = error_of("");
    CHECK(e.pos == 0);
    CHECK(contains(e.message, "empty"));

    e = error_of("   ");
    CHECK(contains(e.message, "empty"));

    e = error_of("1 +");
    CHECK(e.pos == 3);
    CHECK(contains(e.message, "expected a value"));

    e = error_of("u + * v");
    CHECK(e.pos == 4);
    CHECK(e.len == 1);

    e = error_of("1 2");
    CHECK(e.pos == 2);

    e = error_of("u)");
    CHECK(e.pos == 1);
    CHECK(contains(e.message, "')'"));

    e = error_of("(1 + 2");
    CHECK(e.pos == 0);
    CHECK(contains(e.message, "unbalanced"));

    e = error_of("max(1, 2");
    CHECK(e.pos == 3);
    CHECK(contains(e.message, "unbalanced"));

    e = error_of("2 * foo");
    CHECK(e.pos == 4);
    CHECK(e.len == 3);
    CHECK(contains(e.message, "unknown name 'foo'"));

    e = error_of("uv + 1");
    CHECK(e.pos == 0);
    CHECK(e.len == 2);
    CHECK(contains(e.message, "u*v"));

    e = error_of("r");
    CHECK(contains(e.message, "unknown name 'r'"));

    e = error_of("sin u");
    CHECK(e.pos == 0);
    CHECK(e.len == 3);
    CHECK(contains(e.message, "sin(...)"));

    e = error_of("u(2)");
    CHECK(contains(e.message, "variable"));

    e = error_of("pi(2)");
    CHECK(contains(e.message, "constant"));

    e = error_of("atan2(1)");
    CHECK(e.pos == 0);
    CHECK(e.len == 8);
    CHECK(contains(e.message, "2 arguments"));

    e = error_of("sin()");
    CHECK(contains(e.message, "1 argument"));

    e = error_of("2 # 3");
    CHECK(e.pos == 2);
    CHECK(contains(e.message, "'#'"));

    e = error_of("2 * \xce\xbc");  // 2 * μ
    CHECK(e.pos == 4);
    CHECK(contains(e.message, "ASCII"));
}

TEST_CASE("nesting and length limits") {
    std::string ok(kMaxDepth - 1, '(');
    ok += "1" + std::string(kMaxDepth - 1, ')');
    CHECK(eval(ok) == 1);

    std::string deep(kMaxDepth + 1, '(');
    deep += "1" + std::string(kMaxDepth + 1, ')');
    CHECK(contains(error_of(deep).message, "nested too deeply"));

    CHECK(contains(error_of(std::string(kMaxDepth + 1, '-') + "1").message, "nested too deeply"));

    std::string sum = "1";
    for (int i = 0; i < 1000; ++i) sum += "+1";
    CHECK(eval(sum) == 1001);

    CHECK(contains(error_of(std::string(kMaxLength + 1, '1')).message, "too long"));
}

TEST_CASE("a failed compile leaves the program empty") {
    Program p;
    Error e;
    REQUIRE(compile("u + 1", p, e));
    CHECK(p.eval(kVars) == 4);
    CHECK_FALSE(compile("u +", p, e));
    CHECK(p.empty());
    CHECK(p.vars_used() == 0);
}
