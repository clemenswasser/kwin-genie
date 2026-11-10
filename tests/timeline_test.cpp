// SPDX-FileCopyrightText: 2026 Clemens Wasser
// SPDX-License-Identifier: GPL-2.0-or-later
/* Checks the KWin TimeLine contract used by the minimize/restore path. */
#include "effect/timeline.h"
#include "genie_timeline.h"

#include <QCoreApplication>
#include <QEasingCurve>

#include <cmath>
#include <cstdio>
#include <chrono>

using namespace KWin;
using namespace std::chrono_literals;

static int failures = 0;

static void check(qreal got, qreal want, const char *what)
{
    if (std::fabs(got - want) > 1e-6) {
        std::fprintf(stderr, "%s: got %.9f, want %.9f\n", what, got, want);
        ++failures;
    }
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    TimeLine tl(500ms, TimeLine::Forward);
    tl.setEasingCurve(QEasingCurve::Linear);

    tl.setElapsed(0ms);
    check(tl.value(), 0.0, "forward start");
    tl.setElapsed(250ms);
    check(tl.value(), 0.5, "forward middle");
    tl.setElapsed(500ms);
    check(tl.value(), 1.0, "forward end");
    if (!tl.done()) {
        std::fprintf(stderr, "forward: not done at the end\n");
        ++failures;
    }

    tl.setDirection(TimeLine::Backward);
    tl.setElapsed(500ms);
    check(tl.value(), 0.0, "backward end");
    if (!tl.done()) {
        std::fprintf(stderr, "backward: not done at the end\n");
        ++failures;
    }
    tl.setElapsed(250ms);
    check(tl.value(), 0.5, "backward middle");
    tl.setElapsed(0ms);
    check(tl.value(), 1.0, "backward start");

    tl.setDirection(TimeLine::Forward);
    tl.setElapsed(250ms);
    check(tl.value(), 0.5, "interrupt forward");
    tl.toggleDirection();
    check(tl.value(), 0.5, "toggle keeps progress");
    tl.setElapsed(375ms);
    check(tl.value(), 0.25, "backward after toggle");
    tl.toggleDirection();
    check(tl.value(), 0.25, "toggle back keeps progress");
    tl.setElapsed(500ms);
    check(tl.value(), 1.0, "forward after toggle");

    // A completed animation can still be in the effect's held-frames window.
    // Starting the next animation must reset the timeline, not just change its
    // direction, otherwise the done flag survives and the animation is skipped.
    TimeLine restarted(500ms, TimeLine::Forward);
    restarted.setElapsed(500ms);
    startGenieTimeline(restarted, false, 500ms, false);
    check(restarted.value(), 1.0, "restart after done starts minimized");
    if (restarted.done()) {
        std::fprintf(stderr, "restart after done: timeline is still done\n");
        ++failures;
    }
    restarted.setElapsed(250ms);
    check(restarted.value(), 0.5, "restart after done runs backwards");
    restarted.setElapsed(500ms);
    check(restarted.value(), 0.0, "restart after done ends restored");

    if (failures == 0) {
        std::printf("timeline: forward/backward/toggle contract ok\n");
    }
    return failures == 0 ? 0 : 1;
}
