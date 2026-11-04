#version 300 es
// Transform feedback: one vertex per (curve, t sample) -> phi(t).
#include "curve_data.glsl"
#include "cf.glsl"

uniform int u_family;
uniform int u_first;    // first curve in u_params
uniform int u_samples;
uniform vec2 u_trange;  // (t0, dt)

out vec2 o_phi;

void main() {
    int c = u_first + gl_VertexID / u_samples, j = gl_VertexID % u_samples;
    float t = u_trange.x + u_trange.y * float(j);
    o_phi = curve_texel(c, 1).z == 0.0 ? vec2(0.0) : cf(u_family, t, curve_texel(c, 0));
    gl_Position = vec4(0.0);
}
