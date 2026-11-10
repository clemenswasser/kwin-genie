// SPDX-FileCopyrightText: 2026 Clemens Wasser
// SPDX-License-Identifier: GPL-2.0-or-later
/* genie_build_shadow_mesh vs. SkyLight _MatchRectToMeshWithTextureRect
 * (vectors recorded from macOS 26.7.2). */
#include "genie.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static float f(uint32_t u)
{
    float x;
    memcpy(&x, &u, 4);
    return x;
}

int main(int argc, char **argv)
{
    FILE *fp = fopen(argc > 1 ? argv[1] : "tests/shadow_frames.txt", "r");
    if (!fp) {
        perror("open");
        return 2;
    }
    static genie_mesh m;
    static genie_shadow_mesh s;
    int cases = 0, shape_bad = 0;
    double worst_pos = 0, worst_tex = 0;
    int cols, rows;
    while (fscanf(fp, "%d %d", &cols, &rows) == 2) {
        for (int i = 0; i < cols * rows; i++) {
            unsigned a[4];
            if (fscanf(fp, "%u %u %u %u", &a[0], &a[1], &a[2], &a[3]) != 4)
                return 3;
            m.v[i] = (genie_vertex){f(a[0]), f(a[1]), f(a[2]), f(a[3])};
        }
        m.cols = cols;
        m.rows = rows;
        double r[4], t[4];
        int oc, orr;
        if (fscanf(fp, "%lf %lf %lf %lf %lf %lf %lf %lf %d %d",
                   &r[0], &r[1], &r[2], &r[3], &t[0], &t[1], &t[2], &t[3], &oc, &orr) != 10)
            return 3;
        if (genie_build_shadow_mesh(&m, (genie_rect){r[0], r[1], r[2], r[3]},
                                    (genie_rect){t[0], t[1], t[2], t[3]}, &s) != 0) {
            fprintf(stderr, "case %d: build failed\n", cases);
            return 1;
        }
        cases++;
        if (s.cols != oc + 1 || s.rows != orr + 1)
            shape_bad++;
        for (int ci = 0; ci < oc * orr; ci++) {
            unsigned q[16];
            for (int k = 0; k < 16; k++) {
                if (fscanf(fp, "%u", &q[k]) != 1)
                    return 3;
            }
            const int rr = ci / oc, cc = ci % oc;
            const int idx[4] = {rr * s.cols + cc, rr * s.cols + cc + 1,
                                (rr + 1) * s.cols + cc + 1, (rr + 1) * s.cols + cc};
            for (int k = 0; k < 4; k++) {
                const genie_vertex *v = &s.v[idx[k]];
                worst_pos = fmax(worst_pos, fmax(fabs(v->x - f(q[k])), fabs(v->y - f(q[4 + k]))));
                worst_tex = fmax(worst_tex, fmax(fabs(v->u - f(q[8 + k])), fabs(v->v - f(q[12 + k]))));
            }
        }
    }
    printf("shadow: %d meshes, shape mismatch %d, max position diff %.3g px, max texcoord diff %.3g\n",
           cases, shape_bad, worst_pos, worst_tex);
    return (cases > 0 && shape_bad == 0 && worst_pos < 1e-3 && worst_tex < 1e-3) ? 0 : 1;
}
