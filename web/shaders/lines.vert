#version 300 es
// Thick lines: one instance per segment, 6 vertices (two triangles) each.
// Ends are mitred with the neighbouring segments so joints have no gaps.
#include "curve_data.glsl"

uniform int u_first;      // first curve in u_params
uniform int u_samples;
uniform vec4 u_view;      // clip = (z - u_view.xy) * u_view.zw
uniform vec2 u_viewport;  // px
uniform float u_half;     // half width, px, including the AA fringe
uniform vec2 u_fade;      // (k, n): curve i of n gets weight k^(n - 1 - i)

out vec2 v_z;             // phi
out vec3 v_uvt;           // (u_norm, v_norm, t fraction)
out float v_dist;         // signed px from the centre line
out float v_weight;

vec2 to_px(vec2 z) { return (z - u_view.xy) * u_view.zw * 0.5 * u_viewport; }

vec2 normal_of(vec2 d, vec2 fallback) {
    float l = length(d);
    return l > 1e-4 ? vec2(-d.y, d.x) / l : fallback;
}

void main() {
    int segs = u_samples - 1;
    int curve = gl_InstanceID / segs, j = gl_InstanceID % segs, base = curve * u_samples;
    vec4 q = curve_texel(u_first + curve, 1);
    vec2 z0 = point_at(base + j), z1 = point_at(base + j + 1);
    vec2 p0 = to_px(z0), p1 = to_px(z1);
    vec2 pm = to_px(point_at(base + max(j - 1, 0)));
    vec2 pn = to_px(point_at(base + min(j + 2, segs)));

    vec2 d = p1 - p0;
    if (q.z == 0.0 || dot(d, d) < 1e-8) {  // invalid curve or empty segment
        gl_Position = vec4(2.0, 2.0, 2.0, 1.0);
        return;
    }
    vec2 n = normal_of(d, vec2(0.0));
    int end = (gl_VertexID == 1 || gl_VertexID >= 4) ? 1 : 0;
    float side = (gl_VertexID == 2 || gl_VertexID == 3 || gl_VertexID == 5) ? 1.0 : -1.0;
    vec2 other = normal_of(end == 0 ? p0 - pm : pn - p1, n);
    vec2 m = n + other;
    float c = dot(m, n);
    // Mitre when the turn is under ~120 degrees, else a plain butt end.
    vec2 offset = c > 0.5 * length(m) ? m * (u_half / c) : n * u_half;

    vec2 p = (end == 0 ? p0 : p1) + side * offset;
    gl_Position = vec4(p / (0.5 * u_viewport), 0.0, 1.0);
    v_z = end == 0 ? z0 : z1;
    v_uvt = vec3(q.xy, float(j + end) / float(segs));
    v_dist = side * u_half;
    v_weight = pow(u_fade.x, u_fade.y - 1.0 - float(curve));
}
