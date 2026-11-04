#version 300 es
// One instance per curve, one vertex per t sample.
// Per curve: texel 0 = family constants, texel 1 = (u_norm, v_norm, valid, 0).

uniform highp sampler2D u_params;
uniform int u_samples;
uniform vec2 u_trange;
uniform float u_scale;

out float v_frac;

void main() {
    int w = textureSize(u_params, 0).x;
    int i = 2 * gl_InstanceID;
    vec4 p = texelFetch(u_params, ivec2(i % w, i / w), 0);
    vec4 q = texelFetch(u_params, ivec2((i + 1) % w, (i + 1) / w), 0);

    if (q.z == 0.0) {  // invalid curve: outside the clip volume
        gl_Position = vec4(2.0, 2.0, 2.0, 1.0);
        v_frac = 0.0;
        return;
    }

    float t = mix(u_trange.x, u_trange.y, float(gl_VertexID) / float(u_samples - 1));

    // Normal: p = (mu, sigma^2 / 2)
    float r = exp(-p.y * t * t);
    float a = p.x * t;

    gl_Position = vec4(u_scale * r * vec2(cos(a), sin(a)), 0.0, 1.0);
    v_frac = q.x;
}
