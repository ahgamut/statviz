// Offscreen accumulation targets (scrub trails, Render mode, exports), plus
// drawing them to the screen under a different view than they were rendered
// with. key and content describe what a target holds; clear() resets them.
import { loadProgram, uniforms, shaderUrl } from './util.js';

export async function createTrailTools(gl) {
  const [composite, fill] = await Promise.all([
    loadProgram(gl, shaderUrl('screen.vert'), shaderUrl('composite.frag')),
    loadProgram(gl, shaderUrl('screen.vert'), shaderUrl('fill.frag')),
  ]);
  const cu = uniforms(gl, composite, ['u_map', 'u_tex']);
  const vao = gl.createVertexArray();
  // Half floats so fading doesn't stall at 8-bit steps.
  const half = !!(gl.getExtension('EXT_color_buffer_float') || gl.getExtension('EXT_color_buffer_half_float'));
  const format = half ? [gl.RGBA16F, gl.RGBA, gl.HALF_FLOAT] : [gl.RGBA8, gl.RGBA, gl.UNSIGNED_BYTE];

  function createTarget() {
    const tex = gl.createTexture(), fbo = gl.createFramebuffer();
    const target = {
      w: 0, h: 0, view: null, key: null, content: null,
      resize(w, h) {
        if (w === target.w && h === target.h) return;
        target.w = w; target.h = h;
        gl.bindTexture(gl.TEXTURE_2D, tex);
        gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
        gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
        gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
        gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
        gl.texImage2D(gl.TEXTURE_2D, 0, format[0], w, h, 0, format[1], format[2], null);
        gl.bindFramebuffer(gl.FRAMEBUFFER, fbo);
        gl.framebufferTexture2D(gl.FRAMEBUFFER, gl.COLOR_ATTACHMENT0, gl.TEXTURE_2D, tex, 0);
      },
      bind() {
        gl.bindFramebuffer(gl.FRAMEBUFFER, fbo);
        gl.viewport(0, 0, target.w, target.h);
      },
      clear() {
        target.key = target.content = null;
        target.bind();
        gl.clearColor(0, 0, 0, 0);
        gl.clear(gl.COLOR_BUFFER_BIT);
      },
      // Multiplies every channel by f.
      scale(f) {
        target.bind();
        gl.useProgram(fill);
        gl.bindVertexArray(vao);
        gl.enable(gl.BLEND);
        gl.blendColor(0, 0, 0, f);
        gl.blendFunc(gl.ZERO, gl.CONSTANT_ALPHA);
        gl.drawArrays(gl.TRIANGLES, 0, 3);
        gl.bindVertexArray(null);
      },
      dispose() {
        gl.deleteFramebuffer(fbo);
        gl.deleteTexture(tex);
      },
      texture: tex,
    };
    return target;
  }

  return {
    half,
    createTarget,

    // Draws target (rendered under target.view) into the bound framebuffer
    // under view; views are [cx, cy, kx, ky].
    composite(target, view) {
      const [cx, cy, kx, ky] = view, [ox, oy, okx, oky] = target.view;
      gl.useProgram(composite);
      gl.bindVertexArray(vao);
      gl.activeTexture(gl.TEXTURE0);
      gl.bindTexture(gl.TEXTURE_2D, target.texture);
      gl.uniform1i(cu.u_tex, 0);
      gl.uniform4f(cu.u_map, (0.5 * okx) / kx, (0.5 * oky) / ky, 0.5 * (cx - ox) * okx + 0.5, 0.5 * (cy - oy) * oky + 0.5);
      gl.drawArrays(gl.TRIANGLES, 0, 3);
      gl.bindVertexArray(null);
    },
  };
}
