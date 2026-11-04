// Curves as line strips over evaluated points (see evaluate.js).
import { loadProgram, uniforms, shaderUrl } from './util.js';

export async function createCurves(gl) {
  const program = await loadProgram(gl, shaderUrl('curve.vert'), shaderUrl('curve.frag'));
  const u = uniforms(gl, program, ['u_params', 'u_points', 'u_first', 'u_samples', 'u_scale']);
  const vao = gl.createVertexArray();  // no attributes; ids drive everything

  return {
    // Curves [first, first + count) of params, whose points start at 0.
    draw({ params, points, first = 0, count, samples, scale }) {
      if (!count) return;
      gl.useProgram(program);
      gl.bindVertexArray(vao);
      gl.activeTexture(gl.TEXTURE0);
      gl.bindTexture(gl.TEXTURE_2D, params.texture);
      gl.activeTexture(gl.TEXTURE1);
      gl.bindTexture(gl.TEXTURE_2D, points);
      gl.uniform1i(u.u_params, 0);
      gl.uniform1i(u.u_points, 1);
      gl.uniform1i(u.u_first, first);
      gl.uniform1i(u.u_samples, samples);
      gl.uniform1f(u.u_scale, scale);
      gl.drawArraysInstanced(gl.LINE_STRIP, 0, samples, count);
      gl.bindVertexArray(null);
    },
  };
}
