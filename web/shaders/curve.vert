#version 300 es
// One instance per curve, one vertex per t sample, read from evaluated points.
#include "curve_data.glsl"

uniform int u_first;
uniform int u_samples;
uniform float u_scale;

out float v_frac;

void main() {
    vec4 q = curve_texel(u_first + gl_InstanceID, 1);
    if (q.z == 0.0) {  // invalid curve: outside the clip volume
        gl_Position = vec4(2.0, 2.0, 2.0, 1.0);
        v_frac = 0.0;
        return;
    }
    vec2 z = point_at(gl_InstanceID * u_samples + gl_VertexID);
    gl_Position = vec4(u_scale * z, 0.0, 1.0);
    v_frac = q.x;
}
