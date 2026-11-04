#version 300 es
precision highp float;

in float v_frac;
out vec4 o_color;

// Placeholder gradient until colormaps exist.
vec3 palette(float x) {
    return 0.5 + 0.5 * cos(6.2831853 * (0.8 * x + vec3(0.0, 0.33, 0.67)));
}

void main() {
    o_color = vec4(palette(v_frac), 1.0);
}
