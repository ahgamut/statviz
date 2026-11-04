export async function loadText(url) {
  const res = await fetch(url);
  if (!res.ok) throw new Error(`${url}: HTTP ${res.status}`);
  return res.text();
}

export function compileProgram(gl, vsSource, fsSource) {
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
  gl.linkProgram(p);
  gl.deleteShader(vs);
  gl.deleteShader(fs);
  if (!gl.getProgramParameter(p, gl.LINK_STATUS)) {
    throw new Error(`program link failed:\n${gl.getProgramInfoLog(p)}`);
  }
  return p;
}

export async function loadProgram(gl, vsUrl, fsUrl) {
  const [vs, fs] = await Promise.all([loadText(vsUrl), loadText(fsUrl)]);
  return compileProgram(gl, vs, fs);
}

export function uniforms(gl, program, names) {
  const u = {};
  for (const n of names) u[n] = gl.getUniformLocation(program, n);
  return u;
}
