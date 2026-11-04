// Thick antialiased curves drawn from evaluated points (see evaluate.js).
import { loadProgram, uniforms, shaderUrl } from './util.js';

export const COLOR_BY = ['u', 'v', 't', 'abs', 'arg'];

export async function createLines(gl, colormaps) {
  const program = await loadProgram(gl, shaderUrl('lines.vert'), shaderUrl('lines.frag'));
  const u = uniforms(gl, program, [
    'u_params', 'u_points', 'u_first', 'u_offset', 'u_samples', 'u_view', 'u_viewport', 'u_half', 'u_fade',
    'u_cmap', 'u_cmap_row', 'u_cmap_cyclic', 'u_reverse', 'u_color_by', 'u_core', 'u_alpha',
  ]);
  const vao = gl.createVertexArray();  // no attributes; ids drive everything

  return {
    // Curves [first, first + count) of params, whose points start at curve
    // `offset` of the points texture.
    // view: [cx, cy, kx, ky] with clip = (z - c) * k; viewport: [w, h] px.
    // style: {width (px), opacity, colorBy, colormap, reverse};
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
      gl.uniform1i(u.u_first, first);
      gl.uniform1i(u.u_offset, offset);
      gl.uniform1i(u.u_samples, samples);
      gl.uniform4fv(u.u_view, view);
      gl.uniform2fv(u.u_viewport, viewport);
      gl.uniform1f(u.u_core, width / 2);
      gl.uniform1f(u.u_half, width / 2 + 1);
      gl.uniform2fv(u.u_fade, fade);
      gl.uniform1f(u.u_alpha, style.opacity * coverage);
      gl.uniform1f(u.u_cmap_row, (colormaps.row(style.colormap) + 0.5) / colormaps.rows);
      gl.uniform1i(u.u_cmap_cyclic, colormaps.cyclic(style.colormap) ? 1 : 0);
      gl.uniform1i(u.u_reverse, style.reverse ? 1 : 0);
      gl.uniform1i(u.u_color_by, Math.max(0, COLOR_BY.indexOf(style.colorBy)));
      gl.drawArraysInstanced(gl.TRIANGLES, 0, 6, count * (samples - 1));
      gl.bindVertexArray(null);
    },
  };
}
