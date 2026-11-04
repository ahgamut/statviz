#pragma once

#include <string>

namespace sv {

// JSON description of families, parameters and variables for the UI:
// {"families":[{"id","key","name","lattice","latex",
//               "params":[{"name","symbol","domain","domainText","default"}]}],
//  "vars":[...], "curveFloats":N, "maxParams":N}
std::string metadata_json();

}  // namespace sv
