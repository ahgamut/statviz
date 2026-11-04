// Evaluates each family on the GPU for pseudo-random params and compares with
// the C++ double-precision points. window.results holds the outcome.
import { loadEngine } from '../js/engine.js';
import { createEvaluator } from '../js/gl/evaluate.js';
import { createParams } from '../js/gl/params.js';

// Deterministic per-curve value in [lo, hi] from the (u, v) grid point.
const rnd = (k, lo, hi) => `${lo} + ${hi - lo} * mod(sin(12.9898*u + 78.233*v + ${k}) * 43758.5453, 1)`;
const r = (lo, hi) => (k) => rnd(k, lo, hi);
const c = (x) => () => String(x);

// [name, family key, param generators, t0, t1, tolerance]. With 513 samples
// over a power-of-two range every t is exact in float32, so only phi's own
// error is measured; "large phase" shows the float32 limit on mu * t.
const CASES = [
  ['normal', 'normal', [r(-5, 5), r(0, 1)], -16, 16, 3e-5],
  ['normal, large phase', 'normal', [r(-50, 50), r(0, 0.01)], 0, 128, 1e-3],
  ['exponential', 'exponential', [r(0.05, 5)], -32, 32, 3e-5],
  ['gamma', 'gamma', [r(0, 20), r(0, 3)], -16, 16, 3e-5],
  ['chi_squared', 'chi_squared', [r(0, 30)], -8, 8, 3e-5],
  ['uniform', 'uniform', [r(-3, 0), r(0, 3)], -32, 32, 3e-5],
  ['laplace', 'laplace', [r(-5, 5), r(0, 2)], -16, 16, 3e-5],
  ['cauchy', 'cauchy', [r(-5, 5), r(0, 1)], -16, 16, 3e-5],
  ['logistic', 'logistic', [r(-3, 3), r(0, 1)], -16, 16, 3e-5],
  ['stable', 'stable', [r(0.1, 2), r(-1, 1), r(0, 2), r(-2, 2)], -8, 8, 3e-5],
  ['stable, alpha near 1', 'stable', [r(0.999, 1.001), r(-1, 1), r(0, 2), r(-2, 2)], -8, 8, 3e-5],
  ['stable, alpha = 1', 'stable', [c(1), r(-1, 1), r(0, 2), r(-2, 2)], -8, 8, 3e-5],
  ['bernoulli', 'bernoulli', [r(0, 1)], -16, 16, 3e-5],
  ['binomial', 'binomial', [r(0, 200), r(0, 1)], -8, 8, 3e-5],
  ['binomial, large n', 'binomial', [r(500, 2000), r(0, 1)], -8, 8, 3e-4],
  ['poisson', 'poisson', [r(0, 100)], -16, 16, 3e-5],
  ['geometric', 'geometric', [r(0.01, 1)], -16, 16, 3e-5],
  ['neg_binomial', 'neg_binomial', [r(0.1, 50), r(0.05, 1)], -16, 16, 3e-5],
  ['discrete_uniform', 'discrete_uniform', [r(-20, 0), r(0, 20)], -8, 8, 3e-5],
];
const N = 16, SAMPLES = 513;

async function run() {
  const gl = new OffscreenCanvas(1, 1).getContext('webgl2');
  const [engine, evaluator] = await Promise.all([loadEngine(), createEvaluator(gl)]);
  const params = createParams(gl);
  document.getElementById('info').textContent =
    `${gl.getParameter(gl.VERSION)} · ${gl.getParameter(gl.RENDERER)} · ${N * N} curves × ${SAMPLES} samples per case`;
  engine.setSweep('u', 0, 1, N);
  engine.setSweep('v', 0, 1, N);
  const results = [];
  for (const [name, key, gens, t0, t1, tol] of CASES) {
    const fam = engine.meta.families.find((f) => f.key === key);
    engine.setFamily(fam.id);
    gens.forEach((g, i) => {
      const err = engine.setParam(i, g(i + 1));
      if (err) throw new Error(`${name}: ${err.message}`);
    });
    const T = { t0, t1, samples: SAMPLES };
    const { data, stats } = engine.buildGrid();
    params.set(data, stats.curves);
    evaluator.evaluate(params, fam.id, 0, stats.curves, T);
    const gpu = evaluator.read(stats.curves * SAMPLES);
    const cpu = engine.pointsGrid(0, stats.curves, T);
    let worst = 0, at = 0;
    for (let i = 0; i < gpu.length; i += 2) {
      const e = Math.hypot(gpu[i] - cpu[i], gpu[i + 1] - cpu[i + 1]);
      if (!(e <= worst)) { worst = e; at = i / 2; }  // NaN counts as worst
    }
    const curve = Math.floor(at / SAMPLES), j = at % SAMPLES;
    const p = data.subarray(curve * 8, curve * 8 + 4);
    results.push({
      name, curves: stats.valid, err: worst, tol, ok: worst <= tol,
      at: `t=${(t0 + (t1 - t0) * j / (SAMPLES - 1)).toFixed(3)} texel0=[${Array.from(p, (x) => x.toPrecision(5))}]`,
    });
  }
  return results;
}

function show(results) {
  const table = document.getElementById('out');
  for (const r of results) {
    const row = table.insertRow();
    row.className = r.ok ? 'ok' : 'fail';
    for (const v of [r.name, r.curves, r.err.toExponential(2), r.tol.toExponential(0), r.at]) {
      row.insertCell().textContent = v;
    }
  }
}

window.results = run().then((r) => { show(r); return r; }, (e) => {
  document.getElementById('info').textContent = String(e?.stack ?? e);
  throw e;
});
