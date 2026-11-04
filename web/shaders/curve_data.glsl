// Per-curve texels (cpp/include/statviz/gpu.hpp) and evaluated points.
uniform highp sampler2D u_params;  // RGBA32F, 2 texels per curve
uniform highp sampler2D u_points;  // RG32F, (Re, Im) per point, row-major

vec4 curve_texel(int curve, int k) {
    int w = textureSize(u_params, 0).x, i = 2 * curve + k;
    return texelFetch(u_params, ivec2(i % w, i / w), 0);
}

vec2 point_at(int i) {
    int w = textureSize(u_points, 0).x;
    return texelFetch(u_points, ivec2(i % w, i / w), 0).xy;
}
