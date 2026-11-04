// Thick antialiased curves drawn from evaluated points (see evaluate.js).
import { loadProgram, uniforms, shaderUrl } from './util.js';

export const COLOR_BY = ['u', 'v', 't', 'abs', 'arg'];
export const MAX_REPEAT = 360;

// Copies drawn per curve: angle 0 means just the curve.
export const repeats = (style) =>
  (style.repeat?.angle ? Math.min(MAX_REPEAT, Math.max(1, Math.round(style.repeat.n))) : 1);

export async function createLines(gl, colormaps) {
  const program = await loadProgram(gl, shaderUrl('lines.vert'), shaderUrl('lines.frag'));
  const u = uniforms(gl, program, [
    'u_params', 'u_points', 'u_first', 'u_offset', 'u_samples', 'u_view', 'u_viewport', 'u_half', 'u_fade',
    'u_repeat', 'u_turn', 'u_cmap', 'u_cmap_row', 'u_cmap_cyclic', 'u_reverse', 'u_color_by', 'u_core', 'u_alpha',
  ]);
  const vao = gl.createVertexArray();  // no attributes; ids drive everything
  const MAX_INSTANCES = 1 << 22;      // per draw call

  return {
    // Curves [first, first + count) of params, whose points start at curve
    // `offset` of the points texture.
    // view: [cx, cy, kx, ky] with clip = (z - c) * k; viewport: [w, h] px.
    // style: {width (px), opacity, colorBy, colormap, reverse, repeat: {n, angle}};
    // fade: [k, n] per-curve weights (default 1).
    draw({ params, points, first = 0, offset = 0, count, samples, view, viewport, style, fade = [1, 1] }) {
      if (!count || samples < 2) return;
      const width = Math.max(style.width, 1);
      const coverage = Math.min(style.width, 1);
      gl.useProgram(program);
      gl.bindVertexArray(vao);
      gl.activeTexture(gl.TEXTURE0);
      gl.bindTexture(gl.TEXTURE_2D, params.texture);
      gl.activeTexture(gl.TEXTURE1);
      gl.bindTexture(gl.TEXTURE_2D, points);
      gl.activeTexture(gl.TEXTURE2);
      gl.bindTexture(gl.TEXTURE_2D, colormaps.texture);
      gl.uniform1i(u.u_params, 0);
      gl.uniform1i(u.u_points, 1);
      gl.uniform1i(u.u_cmap, 2);
      gl.uniform1i(u.u_samples, samples);
      gl.uniform4fv(u.u_view, view);
      gl.uniform2fv(u.u_viewport, viewport);
      gl.uniform1f(u.u_core, width / 2);
      gl.uniform1f(u.u_half, width / 2 + 1);
      gl.uniform1f(u.u_alpha, style.opacity * coverage);
      gl.uniform1f(u.u_cmap_row, (colormaps.row(style.colormap) + 0.5) / colormaps.rows);
      gl.uniform1i(u.u_cmap_cyclic, colormaps.cyclic(style.colormap) ? 1 : 0);
      gl.uniform1i(u.u_reverse, style.reverse ? 1 : 0);
      gl.uniform1i(u.u_color_by, Math.max(0, COLOR_BY.indexOf(style.colorBy)));
      const reps = repeats(style);
      gl.uniform1i(u.u_repeat, reps);
      gl.uniform1f(u.u_turn, style.repeat?.angle ?? 0);
      // Split by curves so each call stays a bounded amount of work.
      const per = reps * (samples - 1), step = Math.max(1, Math.floor(MAX_INSTANCES / per));
      for (let c = 0; c < count; c += step) {
        gl.uniform1i(u.u_first, first + c);
        gl.uniform1i(u.u_offset, offset + c);
        gl.uniform2f(u.u_fade, fade[0], fade[1] - c);
        gl.drawArraysInstanced(gl.TRIANGLES, 0, 6, Math.min(step, count - c) * per);
      }
      gl.bindVertexArray(null);
    },
  };
}
