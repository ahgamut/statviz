#version 300 es
// Full-screen triangle; v_uv = clip * u_map.xy + u_map.zw.
uniform vec4 u_map;
out vec2 v_uv;

void main() {
    vec2 p = vec2(float((gl_VertexID & 1) << 2) - 1.0, float((gl_VertexID & 2) << 1) - 1.0);
    gl_Position = vec4(p, 0.0, 1.0);
    v_uv = p * u_map.xy + u_map.zw;
}
