/* Checks the KWin TimeLine contract used by the minimize/restore path. */
#include "effect/timeline.h"

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

    if (failures == 0) {
        std::printf("timeline: forward/backward/toggle contract ok\n");
    }
    return failures == 0 ? 0 : 1;
}
