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

    // {data: Float32Array of count * curveFloats, stats: {curves, valid, bad[]}}
    buildGrid(first = 0, count = m._sv_grid_size() - first) {
      const ptr = m._sv_build_grid(first, count);
      const data = m.HEAPF32.subarray(ptr >> 2, (ptr >> 2) + count * curveFloats);
      const sp = m._sv_stats() >> 2;
      const s = m.HEAP32.subarray(sp, sp + 2 + meta.maxParams);
      return { data, stats: { curves: s[0], valid: s[1], bad: Array.from(s.subarray(2)) } };
    },
  };
}
