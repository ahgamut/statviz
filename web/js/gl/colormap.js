// Colormap lookup texture: one 256-texel RGBA8 row per map, plus a custom row.
import { COLORMAPS } from './colormaps.js';

const N = 256;

export const COLORMAP_NAMES = [...COLORMAPS.map((c) => c.name), 'custom'];

// stops: [[x, '#rrggbb'], ...] with x ascending in [0, 1].
export function gradient(stops) {
  const rgb = stops.map(([x, hex]) => [x, [1, 3, 5].map((i) => parseInt(hex.slice(i, i + 2), 16))]);
  const out = new Uint8Array(N * 4);
  for (let i = 0; i < N; i++) {
    const x = i / (N - 1);
    let k = 0;
    while (k < rgb.length - 2 && x > rgb[k + 1][0]) k++;
    const [x0, c0] = rgb[k], [x1, c1] = rgb[Math.min(k + 1, rgb.length - 1)];
    const f = x1 > x0 ? Math.min(1, Math.max(0, (x - x0) / (x1 - x0))) : 0;
    for (let j = 0; j < 3; j++) out[4 * i + j] = Math.round(c0[j] + f * (c1[j] - c0[j]));
    out[4 * i + 3] = 255;
  }
  return out;
}

export function createColormaps(gl) {
  const rows = COLORMAP_NAMES.length;
  const pixels = new Uint8Array(N * 4 * rows);
  COLORMAPS.forEach((c, r) => {
    const rgb = Uint8Array.from(atob(c.rgb), (ch) => ch.charCodeAt(0));
    for (let i = 0; i < N; i++) {
      pixels.set(rgb.subarray(3 * i, 3 * i + 3), 4 * (r * N + i));
      pixels[4 * (r * N + i) + 3] = 255;
    }
  });
  const custom = rows - 1;
  pixels.set(gradient([[0, '#1b1f3a'], [0.5, '#e85d75'], [1, '#ffd166']]), 4 * custom * N);

  const texture = gl.createTexture();
  gl.bindTexture(gl.TEXTURE_2D, texture);
  gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
  gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
  gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.REPEAT);
  gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
  gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA8, N, rows, 0, gl.RGBA, gl.UNSIGNED_BYTE, pixels);

  return {
    texture,
    rows,
    cyclic: (name) => COLORMAPS.find((c) => c.name === name)?.cyclic ?? false,
    row: (name) => Math.max(0, COLORMAP_NAMES.indexOf(name)),
    setCustom(stops) {
      gl.bindTexture(gl.TEXTURE_2D, texture);
      gl.texSubImage2D(gl.TEXTURE_2D, 0, 0, custom, N, 1, gl.RGBA, gl.UNSIGNED_BYTE, gradient(stops));
    },
  };
}
