// cf_dump KEY P1 [P2 ...] < t-values
// Prints "re im" of phi(t) for each t read from stdin (one per line).
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "statviz/families.hpp"

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: cf_dump KEY P1 [P2 ...] < t-values\nfamilies:");
        for (const auto& f : sv::families()) std::fprintf(stderr, " %s", f.key);
        std::fprintf(stderr, "\n");
        return 2;
    }
    const sv::Family* fam = nullptr;
    for (const auto& f : sv::families())
        if (std::strcmp(f.key, argv[1]) == 0) fam = &f;
    if (!fam) {
        std::fprintf(stderr, "unknown family '%s'\n", argv[1]);
        return 2;
    }
    if (argc - 2 != fam->nparams) {
        std::fprintf(stderr, "%s takes %d parameters\n", fam->key, fam->nparams);
        return 2;
    }
    double p[sv::kMaxParams];
    for (int i = 0; i < fam->nparams; ++i) p[i] = std::strtod(argv[2 + i], nullptr);
    if (!sv::is_valid(*fam, p)) {
        std::fprintf(stderr, "parameters out of domain\n");
        return 2;
    }
    double t;
    while (std::scanf("%lf", &t) == 1) {
        sv::cf::cplx z = fam->phi(p, t);
        std::printf("%.17g %.17g\n", z.real(), z.imag());
    }
    return 0;
}
