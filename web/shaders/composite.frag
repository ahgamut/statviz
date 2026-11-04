#version 300 es
precision highp float;

uniform sampler2D u_tex;  // premultiplied
in vec2 v_uv;
out vec4 o_color;

void main() {
    bool inside = all(greaterThanEqual(v_uv, vec2(0.0))) && all(lessThanEqual(v_uv, vec2(1.0)));
    o_color = inside ? clamp(texture(u_tex, v_uv), 0.0, 1.0) : vec4(0.0);
}
