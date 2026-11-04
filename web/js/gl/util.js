export async function loadText(url) {
  const res = await fetch(url);
  if (!res.ok) throw new Error(`${url}: HTTP ${res.status}`);
  return res.text();
}

// options.varyings: transform-feedback outputs (interleaved).
export function compileProgram(gl, vsSource, fsSource, { varyings } = {}) {
  const shader = (type, src) => {
    const s = gl.createShader(type);
    gl.shaderSource(s, src);
    gl.compileShader(s);
    if (!gl.getShaderParameter(s, gl.COMPILE_STATUS)) {
      const log = gl.getShaderInfoLog(s);
      gl.deleteShader(s);
      throw new Error(`shader compile failed:\n${log}`);
    }
    return s;
  };
  const vs = shader(gl.VERTEX_SHADER, vsSource);
  const fs = shader(gl.FRAGMENT_SHADER, fsSource);
  const p = gl.createProgram();
  gl.attachShader(p, vs);
  gl.attachShader(p, fs);
  if (varyings) gl.transformFeedbackVaryings(p, varyings, gl.INTERLEAVED_ATTRIBS);
  gl.linkProgram(p);
  gl.deleteShader(vs);
  gl.deleteShader(fs);
  if (!gl.getProgramParameter(p, gl.LINK_STATUS)) {
    throw new Error(`program link failed:\n${gl.getProgramInfoLog(p)}`);
  }
  return p;
}

// Shader source with `#include "file"` lines replaced, relative to url.
export async function loadShader(url) {
  const src = await loadText(url);
  const parts = await Promise.all(src.split('\n').map((line) => {
    const m = line.match(/^\s*#include\s+"([^"]+)"/);
    return m ? loadShader(new URL(m[1], url)) : line;
  }));
  return parts.join('\n');
}

export async function loadProgram(gl, vsUrl, fsUrl, options) {
  const [vs, fs] = await Promise.all([loadShader(vsUrl), loadShader(fsUrl)]);
  return compileProgram(gl, vs, fs, options);
}

export function uniforms(gl, program, names) {
  const u = {};
  for (const n of names) u[n] = gl.getUniformLocation(program, n);
  return u;
}

export const shaderUrl = (name) => new URL(`../../shaders/${name}`, import.meta.url);
