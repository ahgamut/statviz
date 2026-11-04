#version 300 es

layout(location = 0) in vec2 a_pos;
uniform float u_scale;

void main() {
    gl_Position = vec4(u_scale * a_pos, 0.0, 1.0);
}
