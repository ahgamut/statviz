#!/usr/bin/env python3
"""Cross-check the C++ characteristic functions against scipy.

Each family is compared with E[exp(itX)] computed from scipy's pdf/pmf:
oscillatory quadrature (QAWO/QAWF) for continuous families, pmf sums for
discrete ones. Stable has no closed-form density, so its phi is inverted
numerically and compared with scipy's S0 pdf instead.

Usage: venv/bin/python tools/check_cf.py [path/to/cf_dump]
Needs numpy and scipy; build cf_dump first (tools/build.sh).
"""
import os
import subprocess
import sys
import warnings

import numpy as np
from scipy import integrate, stats

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CF_DUMP = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "build-native", "cf_dump")
TS = np.array([-9.0, -2.3, -0.7, 0.05, 0.4, 1.0, 3.1, 12.0])
RNG = np.random.default_rng(12345)
TRIALS = 6


def cf_dump(key, params, ts):
    out = subprocess.run(
        [CF_DUMP, key, *(f"{float(x):.17g}" for x in params)],
        input="\n".join(f"{float(t):.17g}" for t in ts), capture_output=True, text=True, check=True,
    ).stdout.split()
    v = np.array(out, dtype=float).reshape(-1, 2)
    return v[:, 0] + 1j * v[:, 1]


def half_line(f, c, t, weight):
    """Integral of f(x) w(tx) over [c, inf), w = cos or sin."""
    return integrate.quad(f, c, np.inf, weight=weight, wvar=t, limlst=200)[0]


def cf_continuous(dist, t):
    lo, hi = dist.support()
    c = dist.median()
    f = dist.pdf
    if np.isfinite(lo) and np.isfinite(hi):
        re = integrate.quad(f, lo, hi, weight="cos", wvar=t, limit=400)[0]
        im = integrate.quad(f, lo, hi, weight="sin", wvar=t, limit=400)[0]
        return re + 1j * im
    re = im = 0.0
    if np.isfinite(lo):
        re += integrate.quad(f, lo, c, weight="cos", wvar=t, limit=400)[0]
        im += integrate.quad(f, lo, c, weight="sin", wvar=t, limit=400)[0]
    else:  # (-inf, c] via x = -y
        g = lambda y: f(-y)
        re += half_line(g, -c, t, "cos")
        im -= half_line(g, -c, t, "sin")
    re += half_line(f, c, t, "cos")
    im += half_line(f, c, t, "sin")
    return re + 1j * im


def cf_discrete(dist, t):
    lo, hi = dist.support()
    m, s = dist.mean(), dist.std()
    lo = max(lo, np.floor(m - 40 * s - 50))
    hi = min(hi, np.ceil(m + 40 * s + 50))
    k = np.arange(lo, hi + 1)
    return np.sum(dist.pmf(k) * np.exp(1j * t * k))


def u(a, b):
    return float(RNG.uniform(a, b))


# key -> (sample params, scipy distribution, kind)
FAMILIES = {
    "normal": (lambda: [u(-3, 3), u(0.2, 2)], lambda m, s: stats.norm(m, s), "c"),
    "exponential": (lambda: [u(0.2, 5)], lambda l: stats.expon(scale=1 / l), "c"),
    "gamma": (lambda: [u(0.6, 6), u(0.2, 2)], lambda k, th: stats.gamma(k, scale=th), "c"),
    "chi_squared": (lambda: [u(1, 12)], lambda k: stats.chi2(k), "c"),
    "uniform": (lambda: sorted([u(-3, 3), u(-3, 3)]), lambda a, b: stats.uniform(a, b - a), "c"),
    "laplace": (lambda: [u(-2, 2), u(0.2, 2)], lambda m, b: stats.laplace(m, b), "c"),
    "cauchy": (lambda: [u(-2, 2), u(0.2, 2)], lambda x0, g: stats.cauchy(x0, g), "c"),
    "logistic": (lambda: [u(-2, 2), u(0.2, 2)], lambda m, s: stats.logistic(m, s), "c"),
    "bernoulli": (lambda: [u(0, 1)], lambda p: stats.bernoulli(p), "d"),
    "binomial": (lambda: [float(RNG.integers(1, 60)), u(0, 1)], lambda n, p: stats.binom(int(n), p), "d"),
    "poisson": (lambda: [u(0.1, 40)], lambda l: stats.poisson(l), "d"),
    "geometric": (lambda: [u(0.05, 1)], lambda p: stats.geom(p, loc=-1), "d"),
    "neg_binomial": (lambda: [u(0.3, 12), u(0.1, 0.95)], lambda r, p: stats.nbinom(r, p), "d"),
    "discrete_uniform": (
        lambda: sorted([float(RNG.integers(-6, 7)), float(RNG.integers(-6, 7))]),
        lambda a, b: stats.randint(int(a), int(b) + 1), "d"),
}


def check_closed_forms():
    worst_all = 0.0
    for key, (sample, make, kind) in FAMILIES.items():
        worst = 0.0
        for _ in range(TRIALS):
            p = sample()
            ours = cf_dump(key, p, TS)
            dist = make(*p)
            ref = np.array([(cf_continuous if kind == "c" else cf_discrete)(dist, t) for t in TS])
            worst = max(worst, float(np.max(np.abs(ours - ref))))
        print(f"  {key:18s} max |phi - E[exp(itX)]| = {worst:.2e}")
        worst_all = max(worst_all, worst)
    return worst_all


def check_stable():
    """Invert phi numerically: f(x) = (1/pi) int_0^inf Re[exp(-itx) phi(t)] dt."""
    stats.levy_stable.parameterization = "S0"
    worst = 0.0
    cases = [(1.5, 0.5, 1.0, 0.0), (0.8, -0.7, 0.6, 1.0), (1.0, 0.8, 1.2, -0.5), (1.9, 1.0, 0.7, 0.3),
             (1.2, 0.0, 2.0, 0.0), (0.999, 0.8, 1.0, 0.0)]
    xs = np.array([-2.0, -0.5, 0.0, 0.7, 1.5, 3.0])
    for alpha, beta, g, d in cases:
        tmax = (40.0 ** (1 / alpha)) / g  # |phi| ~ exp(-40) beyond
        ts = np.linspace(0, tmax, 200001)
        phi = cf_dump("stable", [alpha, beta, g, d], ts)
        ours = np.array([integrate.trapezoid(np.real(np.exp(-1j * ts * x) * phi), ts) / np.pi for x in xs])
        # scipy's default "piecewise" pdf snaps alpha near 1 to exactly 1
        # (~1e-4 off at alpha = 0.999); its "dni" (integrating scipy's own
        # phi) is right there but breaks down for alpha > 1.
        stats.levy_stable.pdf_default_method = "dni" if abs(alpha - 1) < 0.05 else "piecewise"
        with warnings.catch_warnings():
            warnings.simplefilter("ignore")
            ref = stats.levy_stable.pdf(xs, alpha, beta, loc=d, scale=g)
        err = float(np.max(np.abs(ours - ref)))
        print(f"  stable a={alpha:<5} b={beta:<4} g={g:<3} d={d:<4} max |pdf - scipy| = {err:.2e}")
        worst = max(worst, err)
    return worst


def main():
    if not os.path.exists(CF_DUMP):
        sys.exit(f"{CF_DUMP} not found; run tools/build.sh")
    print("closed forms vs E[exp(itX)] from scipy pdf/pmf:")
    w1 = check_closed_forms()
    print("stable: numerical inversion of phi vs scipy levy_stable (S0):")
    w2 = check_stable()
    ok = w1 < 1e-7 and w2 < 1e-5
    print("OK" if ok else "FAILED")
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
