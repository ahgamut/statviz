#version 300 es

layout(location = 0) in vec2 a_pos;
uniform vec4 u_view;  // clip = (z - u_view.xy) * u_view.zw

void main() {
    gl_Position = vec4((a_pos - u_view.xy) * u_view.zw, 0.0, 1.0);
}
