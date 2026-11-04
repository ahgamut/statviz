#version 300 es
precision highp float;
out vec4 o_color;
void main() { o_color = vec4(0.0); }  // used with constant-alpha blending to scale the target
