// Characteristic functions in float32, mirroring cpp/include/statviz/cf.hpp.
// p = texel 0 of a curve; layouts are in cpp/include/statviz/gpu.hpp.
// Family ids follow sv::FamilyId.

const float PI = 3.14159265358979;
const float TAU = 6.28318530717959;

// GPU sin/cos/atan are often accurate to only ~1e-4, so these are polynomial.

// a mod 2 pi into [-pi, pi]; 2 pi split in three parts (Cody-Waite).
float reduce_angle(float a) {
    float k = floor(a / TAU + 0.5);
    return ((a - k * 3.140625 * 2.0) - k * 9.67502593994140625e-4 * 2.0) - k * 1.509957990978376432e-7 * 2.0;
}

// (cos a, sin a)
vec2 cis(float a) {
    float k = floor(a * (2.0 / PI) + 0.5);  // quadrant
    float r = ((a - k * 1.5703125) - k * 4.837512969970703125e-4) - k * 7.54978995489188216e-8;
    float z = r * r;
    float s = r + r * z * (-1.0 / 6.0 + z * (1.0 / 120.0 + z * (-1.0 / 5040.0 + z * (1.0 / 362880.0))));
    float c = 1.0 + z * (-0.5 + z * (1.0 / 24.0 + z * (-1.0 / 720.0 + z * (1.0 / 40320.0 + z * (-1.0 / 3628800.0)))));
    int q = int(k - 4.0 * floor(k / 4.0));
    return q == 0 ? vec2(c, s) : q == 1 ? vec2(-s, c) : q == 2 ? vec2(-c, -s) : vec2(s, -c);
}

vec2 polar(float r, float a) { return r * cis(a); }

// atan(y / x) in (-pi, pi]; Cephes atanf polynomial on [0, tan(pi/8)].
float atan2_(float y, float x) {
    float ax = abs(x), ay = abs(y), mx = max(ax, ay);
    if (mx == 0.0) return 0.0;
    float u = min(ax, ay) / mx, base = 0.0;  // u in [0, 1]
    if (u > 0.41421356237) { base = PI / 4.0; u = (u - 1.0) / (u + 1.0); }
    float z = u * u;
    float a = base + ((((8.05374449538e-2 * z - 1.38776856032e-1) * z + 1.99777106478e-1) * z
                     - 3.33329491539e-1) * z * u + u);
    if (ay > ax) a = PI / 2.0 - a;
    if (x < 0.0) a = PI - a;
    return y < 0.0 ? -a : a;
}

// sin(x) / x
float sinc(float x) {
    if (abs(x) < 1.0) {
        float y = x * x;
        return 1.0 + y * (-1.0 / 6.0 + y * (1.0 / 120.0 + y * (-1.0 / 5040.0 + y * (1.0 / 362880.0
             + y * (-1.0 / 39916800.0 + y * (1.0 / 6227020800.0))))));
    }
    return cis(x).y / x;
}

float expm1_(float x) {
    if (abs(x) < 0.5) {
        return x * (1.0 + x / 2.0 * (1.0 + x / 3.0 * (1.0 + x / 4.0 * (1.0 + x / 5.0 * (1.0 + x / 6.0
             * (1.0 + x / 7.0 * (1.0 + x / 8.0 * (1.0 + x / 9.0))))))));
    }
    return exp(x) - 1.0;
}

// log(1 + x), x > -1; 2 atanh(x / (2 + x)) series near 0.
float log1p_(float x) {
    if (abs(x) < 0.1) {
        float y = x / (2.0 + x), z = y * y;
        return 2.0 * y * (1.0 + z * (1.0 / 3.0 + z * (1.0 / 5.0 + z * (1.0 / 7.0 + z / 9.0))));
    }
    return log(1.0 + x);
}

// x / sinh(x)
float x_over_sinh(float x) {
    float ax = abs(x);
    if (ax < 1.0) {
        float y = ax * ax;
        return 1.0 / (1.0 + y * (1.0 / 6.0 + y * (1.0 / 120.0 + y * (1.0 / 5040.0
             + y * (1.0 / 362880.0 + y / 39916800.0)))));
    }
    return 2.0 * ax * exp(-ax) / (1.0 - exp(-2.0 * ax));
}

vec2 cf_stable(float t, vec4 p) {
    // p = (1 - alpha, beta T, gamma, delta)
    float gt = abs(p.z * t);
    if (gt == 0.0) return cis(p.w * t);
    float L = log(gt);
    float re = -exp((1.0 - p.x) * L);
    if (re < -100.0) return vec2(0.0);
    float f = p.x == 0.0 ? L : expm1_(p.x * L);
    return polar(exp(re), re * p.y * f * sign(t) + p.w * t);
}

// Integer-support families: 2 pi periodic, so t is reduced first.
vec2 cf_lattice(int family, float t, vec4 p) {
    float h = 0.5 * reduce_angle(t);  // in [-pi/2, pi/2]
    vec2 e = cis(h);
    float s2 = e.y * e.y, st = 2.0 * e.x * e.y;  // sin^2(t/2), sin t
    switch (family) {
        case 9: // bernoulli (p)
            return vec2(1.0 - 2.0 * p.x * s2, p.x * st);
        case 10: { // binomial (n, p): z = 1 - p + p e^{it}, |z|^2 = 1 - 4pq sin^2(t/2)
            if (p.x == 0.0) return vec2(1.0, 0.0);
            float w = 4.0 * p.y * (1.0 - p.y) * s2;
            if (w >= 1.0) return vec2(0.0);
            return polar(exp(0.5 * p.x * log1p_(-w)), p.x * atan2_(p.y * st, 1.0 - 2.0 * p.y * s2));
        }
        case 11: // poisson (lambda)
            return polar(exp(-2.0 * p.x * s2), p.x * st);
        case 12: case 13: { // geometric (p), neg_binomial (r, p): (1 + a - i b)^-r
            float pp = family == 12 ? p.x : p.y;
            float r = family == 12 ? 1.0 : p.x;
            float q = 1.0 - pp, a = 2.0 * q * s2 / pp, b = q * st / pp;
            return polar(exp(-0.5 * r * log1p_(a * (2.0 + a) + b * b)), r * atan2_(b, 1.0 + a));
        }
        case 14: // discrete_uniform ((a+b)/2, n)
            return polar(sinc(p.y * h) / sinc(h), p.x * 2.0 * h);
    }
    return vec2(0.0);
}

vec2 cf(int family, float t, vec4 p) {
    switch (family) {
        case 0: // normal (mu, sigma^2/2)
            return polar(exp(-p.y * t * t), p.x * t);
        case 1: { // exponential (1/lambda)
            float x = p.x * t;
            return vec2(1.0, x) / (1.0 + x * x);
        }
        case 2: case 3: { // gamma, chi_squared (k, theta)
            float x = p.y * t;
            return polar(exp(-0.5 * p.x * log1p_(x * x)), p.x * atan2_(x, 1.0));
        }
        case 4: // uniform (mid, half)
            return polar(sinc(p.y * t), p.x * t);
        case 5: // laplace (mu, b^2)
            return polar(1.0 / (1.0 + p.y * t * t), p.x * t);
        case 6: // cauchy (x0, gamma)
            return polar(exp(-p.y * abs(t)), p.x * t);
        case 7: // logistic (mu, pi s)
            return polar(x_over_sinh(p.y * t), p.x * t);
        case 8:
            return cf_stable(t, p);
    }
    return cf_lattice(family, t, p);
}
