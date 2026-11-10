// SPDX-FileCopyrightText: 2026 Clemens Wasser
// SPDX-License-Identifier: GPL-2.0-or-later
/* Regression tests for the recovered Dock genie mesh.
 *
 * The golden hashes lock the current bit-exact output. They were generated from
 * the C port after it was checked against live Dock captures (58/58 frames,
 * 0.000000 px); they are a regression lock, not an independent oracle. The
 * invariants around them catch the cases that are easy to break without
 * changing the hash of the normal frames: the overlap/lift phase, the
 * start/end rectangles, and the vertex limits.
 */
#include "genie.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    const char *name;
    genie_rect win;
    genie_box target;
    int dir;
} test_case;

static const test_case cases[] = {
    {"down-normal", {300, 100, 640, 400}, {520, 700, 560, 740}, GENIE_DIR_DOWN},
    {"down-overlap", {300, 600, 640, 400}, {520, 900, 560, 940}, GENIE_DIR_DOWN},
    {"up-normal", {300, 400, 640, 400}, {520, 100, 560, 140}, GENIE_DIR_UP},
    {"up-overlap", {300, 100, 640, 400}, {520, 80, 560, 120}, GENIE_DIR_UP},
    {"right-normal", {100, 300, 400, 640}, {700, 520, 740, 560}, GENIE_DIR_RIGHT},
    {"right-overlap", {100, 300, 400, 640}, {450, 520, 490, 560}, GENIE_DIR_RIGHT},
    {"left-normal", {700, 300, 400, 640}, {100, 520, 140, 560}, GENIE_DIR_LEFT},
    {"left-overlap", {700, 300, 400, 640}, {680, 520, 720, 560}, GENIE_DIR_LEFT},
};

static const float progress_values[] = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f};

typedef struct {
    const char *name;
    float p;
    uint64_t hash;
} golden_entry;

static const golden_entry golden[] = {
#include "mesh_golden.inc"
};

static uint64_t hash_mesh(const genie_mesh *m)
{
    uint64_t h = 1469598103934665603ULL;
    const unsigned char *p = (const unsigned char *)m->v;
    const size_t n = (size_t)m->cols * (size_t)m->rows * sizeof(genie_vertex);
    for (size_t i = 0; i < n; ++i) {
        h ^= p[i];
        h *= 1099511628211ULL;
    }
    return h;
}

static int near(double a, double b)
{
    return fabs(a - b) < 0.01;
}

static void bounds(const genie_mesh *m, double *x0, double *y0, double *x1, double *y1)
{
    *x0 = *y0 = 1e300;
    *x1 = *y1 = -1e300;
    for (int i = 0; i < m->cols * m->rows; ++i) {
        if (m->v[i].x < *x0) *x0 = m->v[i].x;
        if (m->v[i].y < *y0) *y0 = m->v[i].y;
        if (m->v[i].x > *x1) *x1 = m->v[i].x;
        if (m->v[i].y > *y1) *y1 = m->v[i].y;
    }
}

static int check_bounds(const genie_mesh *m, const genie_box *b, const char *what)
{
    double x0, y0, x1, y1;
    bounds(m, &x0, &y0, &x1, &y1);
    if (!near(x0, b->x0) || !near(y0, b->y0) || !near(x1, b->x1) || !near(y1, b->y1)) {
        fprintf(stderr, "%s: bounds (%g,%g)-(%g,%g), want (%g,%g)-(%g,%g)\n",
                what, x0, y0, x1, y1, b->x0, b->y0, b->x1, b->y1);
        return 0;
    }
    return 1;
}

static int check_finite(const genie_mesh *m, const char *what)
{
    for (int i = 0; i < m->cols * m->rows; ++i) {
        if (!isfinite(m->v[i].u) || !isfinite(m->v[i].v) || !isfinite(m->v[i].x) || !isfinite(m->v[i].y)) {
            fprintf(stderr, "%s: non-finite vertex %d\n", what, i);
            return 0;
        }
    }
    return 1;
}

static int check_box(const genie_box *b, float x0, float y0, float x1, float y1, const char *what)
{
    if (!near(b->x0, x0) || !near(b->y0, y0) || !near(b->x1, x1) || !near(b->y1, y1)) {
        fprintf(stderr, "%s: box (%g,%g)-(%g,%g), want (%g,%g)-(%g,%g)\n",
                what, b->x0, b->y0, b->x1, b->y1, x0, y0, x1, y1);
        return 0;
    }
    return 1;
}

static int test_smooth_quality(void)
{
    const genie_quality q = {8.0, 0.0, 0.0, 4096, GENIE_MAX_ROWS};
    const genie_rect win = {2002, 1907, 1794, 1104};
    const genie_box target = {172.4375f, 2114.73f, 213.5625f, 2143.27f};
    const float ps[] = {0.5f, 0.65f};
    int failures = 0;

    for (size_t i = 0; i < sizeof(ps) / sizeof(ps[0]); ++i) {
        genie_mesh mesh;
        if (genie_build_mesh_ex(ps[i], win, target, GENIE_DIR_DOWN, &q, &mesh) != 0) {
            fprintf(stderr, "smooth p=%g: build failed\n", ps[i]);
            ++failures;
            continue;
        }
        if (mesh.rows <= 100) {
            fprintf(stderr, "smooth p=%g: only %d rows\n", ps[i], mesh.rows);
            ++failures;
        }
        double max_seg = 0.0, max_bend = 0.0;
        for (int r = 0; r + 1 < mesh.rows; ++r) {
            const double x0 = mesh.v[2 * r].x, y0 = mesh.v[2 * r].y;
            const double x1 = mesh.v[2 * (r + 1)].x, y1 = mesh.v[2 * (r + 1)].y;
            max_seg = fmax(max_seg, hypot(x1 - x0, y1 - y0));
            if (r > 0) {
                const double xm = mesh.v[2 * (r - 1)].x, ym = mesh.v[2 * (r - 1)].y;
                const double a0 = atan2(y0 - ym, x0 - xm);
                const double a1 = atan2(y1 - y0, x1 - x0);
                max_bend = fmax(max_bend, fabs(a1 - a0) * 180.0 / M_PI);
            }
        }
        if (max_seg > 32.0 || max_bend > 3.5) {
            fprintf(stderr, "smooth p=%g: max segment %.2f px, max bend %.2f deg\n",
                    ps[i], max_seg, max_bend);
            ++failures;
        }
    }
    return failures;
}

static int test_target_rect(void)
{
    int failures = 0;
    genie_box icon;

    icon = genie_icon_rect(128, 128);
    if (!check_box(&icon, 4, 4, 124, 124, "icon 128x128")) ++failures;

    icon = genie_icon_rect(640, 400);
    if (!check_box(&icon, 4, 26.5f, 124, 101.5f, "icon 640x400")) ++failures;

    icon = genie_icon_rect(400, 640);
    if (!check_box(&icon, 26.5f, 4, 101.5f, 124, "icon 400x640")) ++failures;

    if (genie_tile_inset(128.0f) != 3.0f || genie_tile_inset(127.0f) != 2.0f
        || genie_tile_inset(48.0f) != 2.0f || genie_tile_inset(47.0f) != 1.0f) {
        fprintf(stderr, "tile inset thresholds wrong\n");
        ++failures;
    }

    icon = genie_icon_rect(640, 400);
    genie_box target = genie_target_rect((genie_box){0, 0, 40, 40}, icon, 1.0f);
    if (!check_box(&target, 2.25f, 7.28125f, 37.75f, 32.71875f, "target 40x40")) ++failures;

    return failures;
}

int main(int argc, char **argv)
{
    const int print = argc > 1 && strcmp(argv[1], "--print") == 0;
    int failures = 0;
    size_t golden_index = 0;

    for (size_t c = 0; c < sizeof(cases) / sizeof(cases[0]); ++c) {
        const test_case *tc = &cases[c];
        for (size_t pi = 0; pi < sizeof(progress_values) / sizeof(progress_values[0]); ++pi) {
            const float p = progress_values[pi];
            genie_mesh mesh;
            if (genie_build_mesh(p, tc->win, tc->target, tc->dir, &mesh) != 0) {
                fprintf(stderr, "%s p=%g: build failed\n", tc->name, p);
                ++failures;
                continue;
            }
            const uint64_t h = hash_mesh(&mesh);
            if (print) {
                printf("    {\"%s\", %.2ff, 0x%016llxULL},\n", tc->name, p, (unsigned long long)h);
            } else {
                if (golden_index >= sizeof(golden) / sizeof(golden[0])
                    || strcmp(golden[golden_index].name, tc->name) != 0
                    || golden[golden_index].p != p) {
                    fprintf(stderr, "%s p=%g: golden table out of sync\n", tc->name, p);
                    ++failures;
                } else if (golden[golden_index].hash != h) {
                    fprintf(stderr, "%s p=%g: hash 0x%016llx, want 0x%016llx\n", tc->name, p,
                            (unsigned long long)h, (unsigned long long)golden[golden_index].hash);
                    ++failures;
                }
                ++golden_index;
            }
            if (!check_finite(&mesh, tc->name)) {
                ++failures;
            }
            if (mesh.cols <= 0 || mesh.rows <= 0 || mesh.cols * mesh.rows > GENIE_MAX_VERTICES) {
                fprintf(stderr, "%s p=%g: bad mesh size %dx%d\n", tc->name, p, mesh.cols, mesh.rows);
                ++failures;
            }
            if (p == 0.0f) {
                const genie_box window = {(float)tc->win.x, (float)tc->win.y,
                                          (float)(tc->win.x + tc->win.w), (float)(tc->win.y + tc->win.h)};
                if (!check_bounds(&mesh, &window, tc->name)) ++failures;
            }
            if (p == 1.0f && !check_bounds(&mesh, &tc->target, tc->name)) {
                ++failures;
            }
        }
    }

    if (!print) {
        if (golden_index != sizeof(golden) / sizeof(golden[0])) {
            fprintf(stderr, "golden table has %zu entries, used %zu\n",
                    sizeof(golden) / sizeof(golden[0]), golden_index);
            ++failures;
        }

        /* The overlap case must lift the whole window first, then slide. */
        genie_mesh m04, m044, m05;
        genie_build_mesh(0.4f, cases[1].win, cases[1].target, cases[1].dir, &m04);
        genie_build_mesh(0.4444f, cases[1].win, cases[1].target, cases[1].dir, &m044);
        genie_build_mesh(0.5f, cases[1].win, cases[1].target, cases[1].dir, &m05);
        double ax0, ay0, ax1, ay1, cx0, cy0, cx1, cy1, bx0, by0, bx1, by1;
        bounds(&m04, &ax0, &ay0, &ax1, &ay1);
        bounds(&m044, &cx0, &cy0, &cx1, &cy1);
        bounds(&m05, &bx0, &by0, &bx1, &by1);
        if (!near(ay1 - ay0, cases[1].win.h)) {
            fprintf(stderr, "down-overlap p=0.4: span %g, want %g (window must lift rigidly)\n",
                    ay1 - ay0, cases[1].win.h);
            ++failures;
        }
        if (!(ay1 < cases[1].win.y + cases[1].win.h && ay0 < cases[1].win.y)) {
            fprintf(stderr, "down-overlap p=0.4: window did not lift (y %g..%g)\n", ay0, ay1);
            ++failures;
        }
        if (!near(cy1, cases[1].target.y0) || !near(cy1 - cy0, cases[1].win.h)) {
            fprintf(stderr, "down-overlap p=0.4444: bottom %g span %g, want target top %g span %g\n",
                    cy1, cy1 - cy0, cases[1].target.y0, cases[1].win.h);
            ++failures;
        }
        int main_trail = 0;
        for (int i = 0; i < m05.cols * m05.rows; ++i) {
            if (near(m05.v[i].y, cases[1].target.y0)) {
                main_trail = 1;
                break;
            }
        }
        if (!main_trail) {
            fprintf(stderr, "down-overlap p=0.5: no row at target top %g\n", cases[1].target.y0);
            ++failures;
        }
        if (!(by0 > cy0 + 1.0)) {
            fprintf(stderr, "down-overlap p=0.5: slide did not start (lead %g -> %g)\n", cy0, by0);
            ++failures;
        }
        if (!(by1 > cases[1].target.y0)) {
            fprintf(stderr, "down-overlap p=0.5: expected the inside-icon row past the target top\n");
            ++failures;
        }
    }

    if (print) {
        return 0;
    }
    failures += test_target_rect();
    failures += test_smooth_quality();
    if (failures == 0) {
        printf("mesh: %zu golden frames, overlap invariants, target rect and smooth quality ok\n",
               sizeof(golden) / sizeof(golden[0]));
    }
    return failures == 0 ? 0 : 1;
}
