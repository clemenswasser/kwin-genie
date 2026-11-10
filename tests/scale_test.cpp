// SPDX-FileCopyrightText: 2026 Clemens Wasser
// SPDX-License-Identifier: GPL-2.0-or-later
/* Checks the transform of the tiny-window scale fallback. */
#include "genie_scale.h"

#include <cmath>
#include <cstdio>
#include <numbers>

using namespace KWin;

static int failures = 0;

static void check(double got, double want, const char *what)
{
    if (std::fabs(got - want) > 1e-9) {
        std::fprintf(stderr, "%s: got %.12f, want %.12f\n", what, got, want);
        ++failures;
    }
}

int main()
{
    const double fx = 100.0, fy = 200.0, fw = 4.0, fh = 29.0;
    const double tx0 = 100.5, ty0 = 750.0, tx1 = 103.5, ty1 = 788.0;

    GenieScale t = genieScaleTransform(fx, fy, fw, fh, tx0, ty0, tx1, ty1, 0.0);
    check(t.xScale, 1.0, "start x scale");
    check(t.yScale, 1.0, "start y scale");
    check(t.xTranslate, 0.0, "start x translate");
    check(t.yTranslate, 0.0, "start y translate");

    t = genieScaleTransform(fx, fy, fw, fh, tx0, ty0, tx1, ty1, 1.0);
    check(t.xScale, (tx1 - tx0) / fw, "end x scale");
    check(t.yScale, (ty1 - ty0) / fh, "end y scale");
    check(t.xTranslate, tx0 - fx, "end x translate");
    check(t.yTranslate, ty0 - fy, "end y translate");

    // The Dock uses a cosine ease, not linear interpolation. At p = 0.25 the
    // eased fraction is 0.1464 instead of 0.25.
    const double e = 0.5 - 0.5 * std::cos(std::numbers::pi * 0.25);
    t = genieScaleTransform(fx, fy, fw, fh, tx0, ty0, tx1, ty1, 0.25);
    check(t.xScale, (1.0 - e) + e * (tx1 - tx0) / fw, "quarter x scale");
    check(t.yScale, (1.0 - e) + e * (ty1 - ty0) / fh, "quarter y scale");
    check(t.xTranslate, e * (tx0 - fx), "quarter x translate");
    check(t.yTranslate, e * (ty0 - fy), "quarter y translate");

    t = genieScaleTransform(fx, fy, fw, fh, tx0, ty0, tx1, ty1, -1.0);
    check(t.xScale, 1.0, "negative progress clamps");
    t = genieScaleTransform(fx, fy, fw, fh, tx0, ty0, tx1, ty1, 2.0);
    check(t.xScale, (tx1 - tx0) / fw, "overshoot progress clamps");

    t = genieScaleTransform(fx, fy, 0.0, fh, tx0, ty0, tx1, ty1, 0.5);
    check(t.xScale, 1.0, "zero-width frame is a no-op");
    t = genieScaleTransform(fx, fy, fw, fh, tx0, ty0, tx0, ty1, 0.5);
    check(t.yScale, 1.0, "zero-width target is a no-op");

    if (failures == 0) {
        std::printf("scale fallback: cosine ease, endpoints, clamping and degenerate cases ok\n");
    }
    return failures == 0 ? 0 : 1;
}
