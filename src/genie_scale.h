// SPDX-FileCopyrightText: 2026 Clemens Wasser
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <algorithm>
#include <cmath>
#include <numbers>

namespace KWin
{

struct GenieScale {
    double xScale = 1.0;
    double yScale = 1.0;
    double xTranslate = 0.0;
    double yTranslate = 0.0;
};

// The Dock falls back to its scale effect for windows narrower or shorter than
// 5 pt. It interpolates the window corners to the tile image rect with a cosine
// ease instead of warping a mesh. The equivalent WindowPaintData transform is a
// scale plus a translation.
inline GenieScale genieScaleTransform(double frameX, double frameY, double frameW, double frameH,
                                      double targetX0, double targetY0, double targetX1, double targetY1,
                                      double progress)
{
    GenieScale transform;
    if (frameW <= 0.0 || frameH <= 0.0) {
        return transform;
    }
    const double targetW = targetX1 - targetX0;
    const double targetH = targetY1 - targetY0;
    if (targetW <= 0.0 || targetH <= 0.0) {
        return transform;
    }

    const double p = std::clamp(progress, 0.0, 1.0);
    const double e = 0.5 - 0.5 * std::cos(std::numbers::pi * p);
    transform.xScale = (1.0 - e) + e * targetW / frameW;
    transform.yScale = (1.0 - e) + e * targetH / frameH;
    transform.xTranslate = e * (targetX0 - frameX);
    transform.yTranslate = e * (targetY0 - frameY);
    return transform;
}

} // namespace KWin
