#version 300 es
// One instance per curve, one vertex per t sample.

uniform highp sampler2D u_params;
uniform int u_samples;
uniform vec2 u_trange;
uniform float u_scale;
uniform int u_count;

out float v_frac;

void main() {
    int c = gl_InstanceID;
    int w = textureSize(u_params, 0).x;
    vec4 p = texelFetch(u_params, ivec2(c % w, c / w), 0);

    float t = mix(u_trange.x, u_trange.y, float(gl_VertexID) / float(u_samples - 1));

    // Normal: p = (mu, sigma^2 / 2)
    float r = exp(-p.y * t * t);
    float a = p.x * t;

    gl_Position = vec4(u_scale * r * vec2(cos(a), sin(a)), 0.0, 1.0);
    v_frac = u_count > 1 ? float(c) / float(u_count - 1) : 0.0;
}
