// SPDX-FileCopyrightText: 2026 Clemens Wasser
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "effect/timeline.h"

#include <QEasingCurve>

#include <chrono>

namespace KWin
{

// Start a fresh genie timeline. This must reset the timeline first: a completed
// animation can still be in the effect's held-frames window, and setDirection()
// on a done TimeLine with the default Strict target redirect mode leaves it done.
inline void startGenieTimeline(TimeLine &timeline, bool minimizing,
                               std::chrono::milliseconds duration, bool slow)
{
    timeline.reset();
    timeline.setDirection(minimizing ? TimeLine::Forward : TimeLine::Backward);
    timeline.setDuration(slow ? duration * 10 : duration);
    timeline.setEasingCurve(QEasingCurve::Linear);
}

} // namespace KWin
