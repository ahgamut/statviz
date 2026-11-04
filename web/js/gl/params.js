// Per-curve params texture: RGBA32F, two texels per curve (cpp/include/statviz/gpu.hpp).
const TEXELS_PER_CURVE = 2;

export function createParams(gl) {
  const texture = gl.createTexture();
  const maxWidth = gl.getParameter(gl.MAX_TEXTURE_SIZE) & ~1;  // even: a curve's texels share a row

  return {
    texture,
    count: 0,

    // data: Float32Array of n * 8 floats.
    set(data, n) {
      const texels = n * TEXELS_PER_CURVE;
      const w = Math.max(TEXELS_PER_CURVE, Math.min(texels, maxWidth));
      const h = Math.max(1, Math.ceil(texels / w));
      let pixels = data.subarray(0, texels * 4);
      if (w * h > texels) {
        pixels = new Float32Array(w * h * 4);
        pixels.set(data.subarray(0, texels * 4));
      }
      gl.bindTexture(gl.TEXTURE_2D, texture);
      gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.NEAREST);
      gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.NEAREST);
      gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
      gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
      gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA32F, w, h, 0, gl.RGBA, gl.FLOAT, pixels);
      this.count = n;
    },
  };
}
