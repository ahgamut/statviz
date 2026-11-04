// Unit circle and axes, drawn under the curves in the preview.
import { loadProgram, uniforms, shaderUrl as shader } from './util.js';

const SEGMENTS = 256;

export async function createOverlay(gl) {
  const program = await loadProgram(gl, shader('overlay.vert'), shader('overlay.frag'));
  const u = uniforms(gl, program, ['u_scale', 'u_color']);

  const pts = [];
  for (let i = 0; i < SEGMENTS; i++) {
    const a = (2 * Math.PI * i) / SEGMENTS;
    pts.push(Math.cos(a), Math.sin(a));
  }
  pts.push(-1.1, 0, 1.1, 0, 0, -1.1, 0, 1.1);

  const vao = gl.createVertexArray();
  gl.bindVertexArray(vao);
  const buf = gl.createBuffer();
  gl.bindBuffer(gl.ARRAY_BUFFER, buf);
  gl.bufferData(gl.ARRAY_BUFFER, new Float32Array(pts), gl.STATIC_DRAW);
  gl.enableVertexAttribArray(0);
  gl.vertexAttribPointer(0, 2, gl.FLOAT, false, 0, 0);
  gl.bindVertexArray(null);

  return {
    draw({ scale }) {
      gl.useProgram(program);
      gl.bindVertexArray(vao);
      gl.uniform1f(u.u_scale, scale);
      gl.uniform4f(u.u_color, 0.28, 0.3, 0.33, 1.0);
      gl.drawArrays(gl.LINE_LOOP, 0, SEGMENTS);
      gl.uniform4f(u.u_color, 0.17, 0.18, 0.2, 1.0);
      gl.drawArrays(gl.LINES, SEGMENTS, 4);
      gl.bindVertexArray(null);
    },
  };
}
