#include "statviz/expr.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <numbers>

namespace sv::expr {

namespace {

double fmod_floor(double x, double y) { return x - y * std::floor(x / y); }
double sign(double x) { return x > 0 ? 1.0 : (x < 0 ? -1.0 : x); }
double clamp3(double x, double lo, double hi) { return std::min(std::max(x, lo), hi); }

struct Fn1 { const char* name; double (*f)(double); };
struct Fn2 { const char* name; double (*f)(double, double); };
struct Fn3 { const char* name; double (*f)(double, double, double); };

const Fn1 kFn1[] = {
    {"sin", [](double x) { return std::sin(x); }},   {"cos", [](double x) { return std::cos(x); }},
    {"tan", [](double x) { return std::tan(x); }},   {"asin", [](double x) { return std::asin(x); }},
    {"acos", [](double x) { return std::acos(x); }}, {"atan", [](double x) { return std::atan(x); }},
    {"sinh", [](double x) { return std::sinh(x); }}, {"cosh", [](double x) { return std::cosh(x); }},
    {"tanh", [](double x) { return std::tanh(x); }}, {"exp", [](double x) { return std::exp(x); }},
    {"log", [](double x) { return std::log(x); }},   {"log2", [](double x) { return std::log2(x); }},
    {"log10", [](double x) { return std::log10(x); }}, {"sqrt", [](double x) { return std::sqrt(x); }},
    {"abs", [](double x) { return std::abs(x); }},   {"floor", [](double x) { return std::floor(x); }},
    {"ceil", [](double x) { return std::ceil(x); }}, {"round", [](double x) { return std::round(x); }},
    {"sign", sign},
};
const Fn2 kFn2[] = {
    {"atan2", [](double y, double x) { return std::atan2(y, x); }},
    {"min", [](double x, double y) { return std::min(x, y); }},
    {"max", [](double x, double y) { return std::max(x, y); }},
    {"mod", fmod_floor},
    {"pow", [](double x, double y) { return std::pow(x, y); }},
};
const Fn3 kFn3[] = {{"clamp", clamp3}};

template <class T, size_t N>
int lookup(const T (&table)[N], std::string_view name) {
    for (size_t i = 0; i < N; ++i)
        if (name == table[i].name) return static_cast<int>(i);
    return -1;
}

enum class Tok { Number, Ident, Plus, Minus, Star, Slash, Caret, LParen, RParen, Comma, End, Bad };

struct Token {
    Tok kind;
    int pos, len;
    double value = 0;
};

bool is_digit(char c) { return c >= '0' && c <= '9'; }
bool is_ident_start(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }
bool is_ident(char c) { return is_ident_start(c) || is_digit(c); }

// 'name'; appending (not prepending) sidesteps a GCC 12 -Wrestrict false positive.
std::string quoted(std::string_view s) {
    std::string r(1, '\'');
    r += s;
    r += '\'';
    return r;
}

}  // namespace

class Parser {
public:
    Parser(std::string_view src, Program& out, Error& err) : src_(src), out_(out), err_(err) {}

    bool run() {
        out_.code_.clear();
        out_.vars_used_ = 0;
        if (parse()) return true;
        out_.code_.clear();
        out_.vars_used_ = 0;
        return false;
    }

private:
    bool parse() {
        if (src_.size() > static_cast<size_t>(kMaxLength))
            return fail(kMaxLength, 0, "expression too long");
        advance();
        if (tok_.kind == Tok::End) return fail(0, 0, "empty expression");
        if (!additive()) return false;
        if (tok_.kind != Tok::End) return unexpected();
        return true;
    }

    std::string_view src_;
    Program& out_;
    Error& err_;
    size_t at_ = 0;
    Token tok_{Tok::End, 0, 0};
    int depth_ = 0;
    int stack_ = 0;

    bool fail(int pos, int len, std::string msg) {
        err_ = {pos, len, std::move(msg)};
        return false;
    }

    std::string_view text(const Token& t) const { return src_.substr(t.pos, t.len); }

    bool unexpected() {
        if (tok_.kind == Tok::End) return fail(tok_.pos, 0, "unexpected end of expression");
        if (tok_.kind == Tok::Bad) {
            unsigned char c = static_cast<unsigned char>(src_[tok_.pos]);
            if (c >= 0x80) return fail(tok_.pos, 1, "only ASCII characters are allowed");
            return fail(tok_.pos, 1, "unexpected character " + quoted(text(tok_)));
        }
        return fail(tok_.pos, tok_.len, "unexpected " + quoted(text(tok_)));
    }

    void advance() {
        while (at_ < src_.size() && (src_[at_] == ' ' || src_[at_] == '\t')) ++at_;
        int pos = static_cast<int>(at_);
        if (at_ >= src_.size()) {
            tok_ = {Tok::End, pos, 0};
            return;
        }
        char c = src_[at_];
        if (is_digit(c) || (c == '.' && at_ + 1 < src_.size() && is_digit(src_[at_ + 1]))) {
            size_t end = at_;
            while (end < src_.size() && is_digit(src_[end])) ++end;
            if (end < src_.size() && src_[end] == '.') {
                ++end;
                while (end < src_.size() && is_digit(src_[end])) ++end;
            }
            // Exponent only if digits follow, so "2e" is 2 * e.
            if (end < src_.size() && (src_[end] == 'e' || src_[end] == 'E')) {
                size_t k = end + 1;
                if (k < src_.size() && (src_[k] == '+' || src_[k] == '-')) ++k;
                if (k < src_.size() && is_digit(src_[k])) {
                    while (k < src_.size() && is_digit(src_[k])) ++k;
                    end = k;
                }
            }
            // strtod gives 0 / inf on under/overflow; the program never sets a locale.
            std::string num(src_.substr(at_, end - at_));
            tok_ = {Tok::Number, pos, static_cast<int>(end - at_), std::strtod(num.c_str(), nullptr)};
            at_ = end;
            return;
        }
        if (is_ident_start(c)) {
            size_t end = at_;
            while (end < src_.size() && is_ident(src_[end])) ++end;
            tok_ = {Tok::Ident, pos, static_cast<int>(end - at_)};
            at_ = end;
            return;
        }
        Tok k = Tok::Bad;
        int len = 1;
        switch (c) {
            case '+': k = Tok::Plus; break;
            case '-': k = Tok::Minus; break;
            case '*':
                if (at_ + 1 < src_.size() && src_[at_ + 1] == '*') { k = Tok::Caret; len = 2; }
                else k = Tok::Star;
                break;
            case '/': k = Tok::Slash; break;
            case '^': k = Tok::Caret; break;
            case '(': k = Tok::LParen; break;
            case ')': k = Tok::RParen; break;
            case ',': k = Tok::Comma; break;
        }
        tok_ = {k, pos, len};
        at_ += len;
    }

    bool emit(Program::Op op, int index = 0, double value = 0, int stack_delta = 0) {
        out_.code_.push_back({op, static_cast<uint8_t>(index), value});
        stack_ += stack_delta;
        if (stack_ > kMaxStack) return fail(tok_.pos, 0, "expression too complex");
        return true;
    }

    bool enter() {
        if (++depth_ > kMaxDepth) return fail(tok_.pos, tok_.len, "expression nested too deeply");
        return true;
    }

    // additive := term (('+' | '-') term)*
    bool additive() {
        if (!term()) return false;
        while (tok_.kind == Tok::Plus || tok_.kind == Tok::Minus) {
            auto op = tok_.kind == Tok::Plus ? Program::Op::Add : Program::Op::Sub;
            advance();
            if (!term() || !emit(op, 0, 0, -1)) return false;
        }
        return true;
    }

    // term := unary (('*' | '/') unary)*
    bool term() {
        if (!unary()) return false;
        while (tok_.kind == Tok::Star || tok_.kind == Tok::Slash) {
            auto op = tok_.kind == Tok::Star ? Program::Op::Mul : Program::Op::Div;
            advance();
            if (!unary() || !emit(op, 0, 0, -1)) return false;
        }
        return true;
    }

    // unary := ('-' | '+') unary | power      (so -2^2 = -4)
    bool unary() {
        if (tok_.kind == Tok::Minus || tok_.kind == Tok::Plus) {
            bool neg = tok_.kind == Tok::Minus;
            advance();
            if (!enter() || !unary()) return false;
            --depth_;
            return !neg || emit(Program::Op::Neg);
        }
        return power();
    }

    // power := primary ('^' unary)?           (right-associative)
    bool power() {
        if (!primary()) return false;
        if (tok_.kind == Tok::Caret) {
            advance();
            if (!enter() || !unary()) return false;
            --depth_;
            return emit(Program::Op::Pow, 0, 0, -1);
        }
        return true;
    }

    // primary := number [implicit power] | name | name '(' args ')' | '(' additive ')'
    bool primary() {
        switch (tok_.kind) {
            case Tok::Number: {
                double v = tok_.value;
                advance();
                if (!emit(Program::Op::Const, 0, v, 1)) return false;
                // Implicit multiplication: 2u, 2pi, 3(u + v), 2u^2 = 2 * u^2.
                if (tok_.kind == Tok::Ident || tok_.kind == Tok::LParen) {
                    if (!power()) return false;
                    return emit(Program::Op::Mul, 0, 0, -1);
                }
                return true;
            }
            case Tok::Ident: return name();
            case Tok::LParen: {
                int open = tok_.pos;
                advance();
                if (!enter() || !additive()) return false;
                --depth_;
                if (tok_.kind != Tok::RParen) {
                    if (tok_.kind == Tok::End) return fail(open, 1, "unbalanced '('");
                    return fail(tok_.pos, tok_.len, "expected ')'");
                }
                advance();
                return true;
            }
            case Tok::End: return fail(tok_.pos, 0, "expected a value");
            default: return unexpected();
        }
    }

    bool name() {
        Token id = tok_;
        std::string_view s = text(id);
        advance();
        bool call = tok_.kind == Tok::LParen;

        for (int i = 0; i < kNumVars; ++i) {
            if (s == kVarNames[i]) {
                if (call) return fail(id.pos, id.len, quoted(s) + " is a variable, not a function");
                out_.vars_used_ |= 1u << i;
                return emit(Program::Op::Var, i, 0, 1);
            }
        }
        if (s == "pi" || s == "e") {
            if (call) return fail(id.pos, id.len, quoted(s) + " is a constant, not a function");
            return emit(Program::Op::Const, 0, s == "pi" ? std::numbers::pi : std::numbers::e, 1);
        }

        int arity = 0, index = -1;
        if ((index = lookup(kFn1, s)) >= 0) arity = 1;
        else if ((index = lookup(kFn2, s)) >= 0) arity = 2;
        else if ((index = lookup(kFn3, s)) >= 0) arity = 3;

        if (arity == 0) {
            bool all_vars = s.size() > 1 && std::all_of(s.begin(), s.end(), [](char c) {
                return std::string_view("uvabcdR").find(c) != std::string_view::npos;
            });
            std::string msg = "unknown name '" + std::string(s) + "'";
            if (all_vars) {
                std::string prod(1, s[0]);
                for (size_t i = 1; i < s.size(); ++i) (prod += '*') += s[i];
                msg += " (write " + prod + " for a product)";
            }
            return fail(id.pos, id.len, msg);
        }
        if (!call) return fail(id.pos, id.len, quoted(s) + " is a function: write " + std::string(s) + "(...)");

        int open = tok_.pos;
        advance();
        if (!enter()) return false;
        int args = 0;
        if (tok_.kind != Tok::RParen) {
            for (;;) {
                if (!additive()) return false;
                ++args;
                if (tok_.kind != Tok::Comma) break;
                advance();
            }
        }
        --depth_;
        if (tok_.kind != Tok::RParen) {
            if (tok_.kind == Tok::End) return fail(open, 1, "unbalanced '('");
            return fail(tok_.pos, tok_.len, "expected ',' or ')'");
        }
        int close = tok_.pos;
        advance();
        if (args != arity) {
            return fail(id.pos, close + 1 - id.pos,
                        quoted(s) + " takes " + std::to_string(arity) +
                            (arity == 1 ? " argument" : " arguments"));
        }
        auto op = arity == 1 ? Program::Op::Call1 : arity == 2 ? Program::Op::Call2 : Program::Op::Call3;
        return emit(op, index, 0, 1 - arity);
    }
};

bool compile(std::string_view src, Program& out, Error& err) { return Parser(src, out, err).run(); }

double Program::eval(const double* vars) const {
    double st[kMaxStack];
    int sp = 0;
    for (const Ins& in : code_) {
        switch (in.op) {
            case Op::Const: st[sp++] = in.value; break;
            case Op::Var: st[sp++] = vars[in.index]; break;
            case Op::Neg: st[sp - 1] = -st[sp - 1]; break;
            case Op::Add: --sp; st[sp - 1] += st[sp]; break;
            case Op::Sub: --sp; st[sp - 1] -= st[sp]; break;
            case Op::Mul: --sp; st[sp - 1] *= st[sp]; break;
            case Op::Div: --sp; st[sp - 1] /= st[sp]; break;
            case Op::Pow: --sp; st[sp - 1] = std::pow(st[sp - 1], st[sp]); break;
            case Op::Call1: st[sp - 1] = kFn1[in.index].f(st[sp - 1]); break;
            case Op::Call2: --sp; st[sp - 1] = kFn2[in.index].f(st[sp - 1], st[sp]); break;
            case Op::Call3: sp -= 2; st[sp - 1] = kFn3[in.index].f(st[sp - 1], st[sp], st[sp + 1]); break;
        }
    }
    return sp == 1 ? st[0] : NAN;
}

}  // namespace sv::expr
