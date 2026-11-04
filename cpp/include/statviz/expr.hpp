#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

// Real-valued parameter expressions: parsed once, evaluated per curve.
namespace sv::expr {

// Variables an expression may use, in this order in the vars array.
enum Var : int { U, V, A, B, C, D, R, kNumVars };
inline constexpr const char* kVarNames[kNumVars] = {"u", "v", "a", "b", "c", "d", "R"};

inline constexpr int kMaxLength = 4096;
inline constexpr int kMaxDepth = 64;   // nesting of parentheses / unary ops
inline constexpr int kMaxStack = 256;  // evaluation stack

// pos and len are byte offsets into the source. Sources are ASCII-only
// (anything else is an error at its first byte), so they equal JS string
// indices.
struct Error {
    int pos = 0;
    int len = 0;
    std::string message;
};

class Program {
public:
    double eval(const double* vars) const;  // vars has kNumVars entries
    unsigned vars_used() const { return vars_used_; }  // bit i = Var i
    bool empty() const { return code_.empty(); }

private:
    friend class Parser;
    enum class Op : uint8_t { Const, Var, Neg, Add, Sub, Mul, Div, Pow, Call1, Call2, Call3 };
    struct Ins {
        Op op;
        uint8_t index;  // Var or function index
        double value;   // Const
    };
    std::vector<Ins> code_;
    unsigned vars_used_ = 0;
};

// Returns false and fills err on a syntax error.
bool compile(std::string_view src, Program& out, Error& err);

}  // namespace sv::expr
