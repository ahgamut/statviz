// Curves of phi(t) evaluated in the vertex shader from a per-curve params
// texture (RGBA32F, one texel per curve).
import { loadProgram, uniforms } from './util.js';

const shader = (name) => new URL(`../../shaders/${name}`, import.meta.url);

export async function createCurves(gl) {
  const program = await loadProgram(gl, shader('curve.vert'), shader('curve.frag'));
  const u = uniforms(gl, program, ['u_params', 'u_samples', 'u_trange', 'u_scale', 'u_count']);
  const vao = gl.createVertexArray();  // no attributes; ids drive everything
  const tex = gl.createTexture();
  const maxWidth = gl.getParameter(gl.MAX_TEXTURE_SIZE);
  let count = 0;

  return {
    // texels: Float32Array of count * 4 floats.
    setParams(texels, n) {
      const w = Math.min(n, maxWidth);
      const h = Math.ceil(n / w);
      let data = texels.subarray(0, n * 4);
      if (w * h > n) {
        data = new Float32Array(w * h * 4);
        data.set(texels.subarray(0, n * 4));
      }
      gl.bindTexture(gl.TEXTURE_2D, tex);
      gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.NEAREST);
      gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.NEAREST);
      gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
      gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
      gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA32F, w, h, 0, gl.RGBA, gl.FLOAT, data);
      count = n;
    },

    draw({ t0, t1, samples, scale }) {
      if (!count) return;
      gl.useProgram(program);
      gl.bindVertexArray(vao);
      gl.activeTexture(gl.TEXTURE0);
      gl.bindTexture(gl.TEXTURE_2D, tex);
      gl.uniform1i(u.u_params, 0);
      gl.uniform1i(u.u_samples, samples);
      gl.uniform2f(u.u_trange, t0, t1);
      gl.uniform1f(u.u_scale, scale);
      gl.uniform1i(u.u_count, count);
      gl.drawArraysInstanced(gl.LINE_STRIP, 0, samples, count);
      gl.bindVertexArray(null);
    },
  };
}
