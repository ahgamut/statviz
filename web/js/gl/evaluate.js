// phi(t) for every (curve, t sample), written by transform feedback into a
// buffer, then copied into an RG32F texture that the line shaders read.
import { loadProgram, uniforms, shaderUrl } from './util.js';

const ROW = 4096;

export async function createEvaluator(gl, { maxPoints = 1 << 22 } = {}) {
  const program = await loadProgram(gl, shaderUrl('eval.vert'), shaderUrl('eval.frag'), { varyings: ['o_phi'] });
  const u = uniforms(gl, program, ['u_params', 'u_family', 'u_first', 'u_samples', 'u_trange']);
  const maxSize = gl.getParameter(gl.MAX_TEXTURE_SIZE);
  const width = Math.min(ROW, maxSize);
  const capacity = width * Math.min(maxSize, Math.ceil(maxPoints / width));

  const vao = gl.createVertexArray();
  const tf = gl.createTransformFeedback();
  const buffer = gl.createBuffer();
  const texture = gl.createTexture();
  let rows = 0;

  function reserve(n) {
    if (n > capacity) throw new Error(`${n} points exceed the evaluator capacity ${capacity}`);
    const need = Math.ceil(n / width);
    if (need <= rows) return;
    rows = Math.max(need, Math.min(2 * rows, Math.ceil(capacity / width)));
    gl.bindBuffer(gl.ARRAY_BUFFER, buffer);
    gl.bufferData(gl.ARRAY_BUFFER, rows * width * 8, gl.DYNAMIC_COPY);
    gl.bindBuffer(gl.ARRAY_BUFFER, null);
    gl.bindTexture(gl.TEXTURE_2D, texture);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.NEAREST);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.NEAREST);
    gl.texImage2D(gl.TEXTURE_2D, 0, gl.RG32F, width, rows, 0, gl.RG, gl.FLOAT, null);
  }

  function toTexture(n) {
    gl.bindBuffer(gl.PIXEL_UNPACK_BUFFER, buffer);
    gl.bindTexture(gl.TEXTURE_2D, texture);
    gl.texSubImage2D(gl.TEXTURE_2D, 0, 0, 0, width, Math.ceil(n / width), gl.RG, gl.FLOAT, 0);
    gl.bindBuffer(gl.PIXEL_UNPACK_BUFFER, null);
  }

  return {
    capacity,
    texture,  // point i at (i % width, i / width)

    // Curves [first, first + count) of params -> points [0, count * samples).
    evaluate(params, family, first, count, { t0, t1, samples }) {
      const n = count * samples;
      if (!n) return;
      reserve(n);
      gl.useProgram(program);
      gl.activeTexture(gl.TEXTURE0);
      gl.bindTexture(gl.TEXTURE_2D, params.texture);
      gl.uniform1i(u.u_params, 0);
      gl.uniform1i(u.u_family, family);
      gl.uniform1i(u.u_first, first);
      gl.uniform1i(u.u_samples, samples);
      gl.uniform2f(u.u_trange, t0, samples > 1 ? (t1 - t0) / (samples - 1) : 0);
      gl.bindVertexArray(vao);
      gl.enable(gl.RASTERIZER_DISCARD);
      gl.bindTransformFeedback(gl.TRANSFORM_FEEDBACK, tf);
      gl.bindBufferBase(gl.TRANSFORM_FEEDBACK_BUFFER, 0, buffer);
      gl.beginTransformFeedback(gl.POINTS);
      gl.drawArrays(gl.POINTS, 0, n);
      gl.endTransformFeedback();
      gl.bindBufferBase(gl.TRANSFORM_FEEDBACK_BUFFER, 0, null);
      gl.bindTransformFeedback(gl.TRANSFORM_FEEDBACK, null);
      gl.disable(gl.RASTERIZER_DISCARD);
      gl.bindVertexArray(null);
      toTexture(n);
    },

    // CPU path: n (Re, Im) points from engine.pointsGrid / pointsScrub.
    upload(data, n) {
      if (!n) return;
      reserve(n);
      gl.bindBuffer(gl.ARRAY_BUFFER, buffer);
      gl.bufferSubData(gl.ARRAY_BUFFER, 0, data, 0, 2 * n);
      gl.bindBuffer(gl.ARRAY_BUFFER, null);
      toTexture(n);
    },

    // Reads back the first n points (tests).
    read(n) {
      const out = new Float32Array(2 * n);
      gl.bindBuffer(gl.ARRAY_BUFFER, buffer);
      gl.getBufferSubData(gl.ARRAY_BUFFER, 0, out);
      gl.bindBuffer(gl.ARRAY_BUFFER, null);
      return out;
    },
  };
}
