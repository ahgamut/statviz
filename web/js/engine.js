// Wrapper around the WebAssembly module. Returned arrays are views into
// WASM memory, valid until the next call that writes the same buffer.
import createModule from '../wasm/statviz.js';

export async function loadEngine() {
  const m = await createModule();
  const texelFloats = m._sv_texel_floats();

  return {
    texelFloats,

    // n Normal curves, sigma linear in [s0, s1]; one texel per curve.
    normalSweep(mu, s0, s1, n) {
      const ptr = m._sv_normal_sweep(mu, s0, s1, n);
      if (!ptr) throw new Error(`normalSweep: invalid curve count ${n}`);
      const i = ptr >> 2;
      return m.HEAPF32.subarray(i, i + n * texelFloats);
    },
  };
}
