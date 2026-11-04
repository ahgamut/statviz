#version 300 es
precision highp float;

uniform sampler2D u_cmap;
uniform float u_cmap_row;   // texture y of the colormap row
uniform int u_cmap_cyclic;
uniform int u_reverse;
uniform int u_color_by;     // 0 u, 1 v, 2 t, 3 |phi|, 4 arg phi
uniform float u_core;       // half width, px, without the fringe
uniform float u_alpha;      // opacity times coverage for sub-pixel widths

in vec2 v_z;
in vec3 v_uvt;
in float v_dist;
in float v_weight;
out vec4 o_color;

void main() {
    float x = u_color_by == 0 ? v_uvt.x
            : u_color_by == 1 ? v_uvt.y
            : u_color_by == 2 ? v_uvt.z
            : u_color_by == 3 ? length(v_z)
            : atan(v_z.y, v_z.x) / 6.28318530718 + 0.5;
    x = clamp(x, 0.0, 1.0);
    if (u_reverse == 1) x = 1.0 - x;
    float s = u_cmap_cyclic == 1 ? x + 0.5 / 256.0 : (x * 255.0 + 0.5) / 256.0;
    vec3 rgb = texture(u_cmap, vec2(s, u_cmap_row)).rgb;
    float a = u_alpha * v_weight * clamp(u_core + 0.5 - abs(v_dist), 0.0, 1.0);
    o_color = vec4(rgb * a, a);  // premultiplied
}
