#pragma once

#include "statviz/scene.hpp"
#include "statviz/scrub.hpp"

// CPU path: phi(t) evaluated in double for each curve and t sample. The
// reference for the GPU path, and the fallback for families without GLSL.
namespace sv {

// Writes count * ts.m (Re, Im) float pairs, curve-major. Invalid curves
// are all zeros (the shader hides them via the validity texel).
void generate_points(const Scene& sc, const State* states, int count, const TRange& ts, float* out);

}  // namespace sv
