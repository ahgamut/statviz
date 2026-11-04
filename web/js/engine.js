// Wrapper around the WebAssembly module. Returned arrays are views into
// WASM memory, valid until the next call that writes the same buffer.
import createModule from '../wasm/statviz.js';

const encoder = new TextEncoder();
const decoder = new TextDecoder();

export async function loadEngine() {
  const m = await createModule();

  const cstring = (ptr) => {
    let end = ptr;
    while (m.HEAPU8[end]) end++;
    return decoder.decode(m.HEAPU8.subarray(ptr, end));
  };
  const writeScratch = (s) => {
    const bytes = encoder.encode(s);
    const ptr = m._sv_scratch(bytes.length);
    m.HEAPU8.set(bytes, ptr);
    return bytes.length;
  };

  const meta = JSON.parse(cstring(m._sv_metadata()));
  const varIndex = Object.fromEntries(meta.vars.map((name, i) => [name, i]));
  const curveFloats = meta.curveFloats;
  let pngWidth = 0;

  return {
    meta,
    curveFloats,

    setFamily(id) {
      if (m._sv_set_family(id) !== 0) throw new Error(`unknown family ${id}`);
    },

    // Returns null, or {pos, len, message} with positions as string indices.
    setParam(index, source) {
      const len = writeScratch(source);
      if (m._sv_set_param(index, len) === 0) return null;
      return { pos: m._sv_error_pos(), len: m._sv_error_len(), message: cstring(m._sv_error_msg()) };
    },

    // Names of the variables the current expressions use.
    varsUsed() {
      const mask = m._sv_vars_used();
      return meta.vars.filter((_, i) => mask & (1 << i));
    },

    setVar(name, value) {
      m._sv_set_var(varIndex[name], value);
    },

    setSweep(axis, lo, hi, n) {
      m._sv_set_sweep(axis === 'v' ? 1 : 0, lo, hi, n);
    },

    setPolicy(policy) {
      m._sv_set_policy(policy === 'clamp' ? 1 : 0);
    },

    gridSize() {
      return m._sv_grid_size();
    },

    maxCurves: m._sv_max_curves(),

    // {data: Float32Array of stats.curves * curveFloats, stats: {curves, valid, bad[]}}.
    // Builds at most maxCurves; build larger grids in slabs.
    buildGrid(first = 0, count = m._sv_grid_size() - first) {
      m._sv_build_grid(first, count);
      return built();
    },

    // Scrub mode. state: {u, v, a, b, c, d, R} (missing names are 0).
    // maxStep: largest allowed gap between neighbouring curves, in unit-disk units.
    // Returns the curves leading from the previous key to this one, plus
    // keys[i]: the key index each curve belongs to.
    scrubPush(state, { t0, t1, samples }, maxStep) {
      const sp = m._sv_state() >> 3;
      meta.vars.forEach((name, i) => { m.HEAPF64[sp + i] = state[name] ?? 0; });
      const n = m._sv_scrub_push(t0, t1, samples, maxStep);
      return { ...built(), keys: curveKeys(n) };
    },

    // Rebuilds keys [k0, k1) at a different maxStep (e.g. export resolution).
    // May stop early to bound memory; continue from the returned next key.
    scrubReplay(k0, k1, { t0, t1, samples }, maxStep) {
      const n = m._sv_scrub_replay(k0, k1, t0, t1, samples, maxStep);
      return { ...built(), keys: curveKeys(n), next: m._sv_scrub_replay_next() };
    },

    scrubClear() {
      m._sv_scrub_clear();
    },

    scrubSize() {
      return m._sv_scrub_size();
    },

    maxPoints: m._sv_max_points(),

    // CPU path: Float32Array of count * samples (Re, Im) pairs, curve-major,
    // for grid curves [first, first + count). count * samples <= maxPoints.
    pointsGrid(first, count, { t0, t1, samples }) {
      return points(m._sv_points_grid(first, count, t0, t1, samples), count * samples);
    },

    // Streaming PNG encoder. Each call returns a copy of the bytes it
    // encoded. text: [keyword, value] ASCII chunks. rows(rgba, n) takes n
    // rows of w RGBA pixels, top to bottom.
    pngBegin(w, h, alpha, text = []) {
      pngWidth = w;
      const parts = [png(m._sv_png_begin(w, h, alpha ? 1 : 0), 'begin')];
      for (const [key, value] of text) {
        const len = writeScratch(key + '\0' + value);
        parts.push(png(m._sv_png_text(len), 'text'));
      }
      return parts;
    },
    pngRows(rgba, n) {
      m.HEAPU8.set(rgba.subarray(0, n * pngWidth * 4), m._sv_png_input(n));
      return png(m._sv_png_rows(n), 'rows');
    },
    pngEnd() {
      return png(m._sv_png_end(), 'end');
    },

    // Same for the curves of the last scrubPush / scrubReplay.
    pointsScrub(count, { t0, t1, samples }) {
      return points(m._sv_points_scrub(t0, t1, samples), count * samples);
    },
  };

  function points(ptr, n) {
    if (!ptr) throw new Error(`too many points (${n}); the limit is ${m._sv_max_points()} per call`);
    return m.HEAPF32.subarray(ptr >> 2, (ptr >> 2) + 2 * n);
  }

  function png(len, step) {
    if (len < 0) throw new Error(`PNG encoder: ${step} failed`);
    return m.HEAPU8.slice(m._sv_png_out(), m._sv_png_out() + len);
  }

  function built() {
    const sp = m._sv_stats() >> 2;
    const s = m.HEAP32.subarray(sp, sp + 2 + meta.maxParams);
    const p = m._sv_curves() >> 2;
    const data = m.HEAPF32.subarray(p, p + s[0] * curveFloats);
    return { data, stats: { curves: s[0], valid: s[1], bad: Array.from(s.subarray(2)) } };
  }

  function curveKeys(count) {
    const p = m._sv_scrub_curve_keys() >> 2;
    return m.HEAP32.subarray(p, p + count);
  }
}
