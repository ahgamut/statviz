#version 300 es
precision highp float;
// Export: the target composited onto the background, flipped so rows read
// back top first, in straight alpha.
uniform sampler2D u_tex;  // premultiplied
uniform vec4 u_bg;        // premultiplied; 0 when transparent
uniform bool u_add;
out vec4 o_color;

void main() {
    ivec2 size = textureSize(u_tex, 0), p = ivec2(gl_FragCoord.xy);
    vec4 c = clamp(texelFetch(u_tex, ivec2(p.x, size.y - 1 - p.y), 0), 0.0, 1.0);
    vec4 o = u_add ? min(u_bg + c, 1.0) : c + u_bg * (1.0 - c.a);
    // Additive colour can exceed alpha: keep how it looks over black.
    o.a = max(o.a, max(o.r, max(o.g, o.b)));
    o_color = o.a > 0.0 ? vec4(o.rgb / o.a, o.a) : vec4(0.0);
}
