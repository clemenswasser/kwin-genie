// SPDX-FileCopyrightText: 2026 Clemens Wasser
// SPDX-License-Identifier: GPL-2.0-or-later
#include "genie.h"
#include <math.h>

static const double genie_pi_d = M_PI;
static const double genie_pi_f = (double)(float)M_PI;

static double genie__ease_profile(double y, double base, double len)
{
    return cos(((y - base) * genie_pi_f) / len) * -0.5 + 0.5;
}

const genie_quality genie_quality_exact = {8.0, 0.6, 128.0, 64, 400};

static int genie__subdivide(double p1, double p2, double base, double L, double R, float *rows, int idx, int len_i, int *counter, const genie_quality *q)
{
    double len = (double)len_i;
    double e_end = genie__ease_profile(p2, base, len);
    int total = 0;
    int ok = *counter < q->max_visits;
    (*counter)++;
    if (!ok)
        return 0;
    for (;;) {
        double span = fabs(p2 - p1);
        if (span < q->min_span)
            return total;
        double mid = (p2 + p1) * 0.5;
        double c1 = cos(((p1 - base) * genie_pi_f) / len);
        double em = genie__ease_profile(mid, base, len);
        float xm = (float)(em * L), ym = (float)(em * R);
        if (span < q->chord_max_span && q->chord_tolerance > 0.0) {
            double e1 = c1 * -0.5 + 0.5;
            float sl = (float)(L * e1) + (float)(e_end * L);
            float sr = (float)(e1 * R) + (float)(R * e_end);
            if (fabs((double)sl * 0.5 - (double)xm) < q->chord_tolerance
                && fabs((double)sr * 0.5 - (double)ym) < q->chord_tolerance)
                return total;
        }
        int n = genie__subdivide(p1, mid, base, L, R, rows, idx, len_i, counter, q);
        unsigned at = (unsigned)(idx + n);
        if (at > (unsigned)(q->max_rows * 3 - 1))
            return total;
        rows[at] = (float)mid;
        rows[at + 1] = xm;
        rows[at + 2] = ym;
        if (at > (unsigned)(q->max_rows * 3 - 4))
            return total;
        idx = (int)at + 3;
        total += n + 3;
        ok = *counter < q->max_visits;
        (*counter)++;
        if (!ok)
            return total;
        p1 = mid;
    }
}

int genie_build_mesh(float p, genie_rect window, genie_box target, int direction, genie_mesh *out)
{
    return genie_build_mesh_ex(p, window, target, direction, &genie_quality_exact, out);
}

int genie_build_mesh_ex(float p, genie_rect W, genie_box T, int dir, const genie_quality *quality, genie_mesh *out)
{
    genie_quality q = *quality;
    if (q.max_rows < 1)
        q.max_rows = 1;
    if (q.max_rows > GENIE_MAX_ROWS)
        q.max_rows = GENIE_MAX_ROWS;
    if (q.max_visits < 1)
        q.max_visits = 1;
    if (q.min_span < 0.0)
        q.min_span = 0.0;
    if (q.chord_tolerance < 0.0)
        q.chord_tolerance = 0.0;
    if (q.chord_max_span < 0.0)
        q.chord_max_span = 0.0;

    const double wx = W.x, wy = W.y, ww = W.w, wh = W.h;
    const double tx = T.x0, ty = T.y0, tw = (double)(T.x1 - T.x0), th = (double)(T.y1 - T.y0);
    double L0, R0, D = 0.0, A, K, E, S0, F;
    switch (dir) {
    case GENIE_DIR_UP:
        L0 = tx - wx;
        R0 = (tw + tx) - (wx + ww);
        K = -wh;
        E = th + ty;
        S0 = wy + wh;
        F = ty;
        if (0.0 <= E - wy)
            D = E - wy;
        A = (D <= 0.0) ? E - S0 : -wh;
        break;
    case GENIE_DIR_DOWN:
        L0 = tx - wx;
        R0 = (tw + tx) - (wx + ww);
        K = wh;
        E = ty;
        S0 = wy;
        F = ty + th;
        if (ty - (wy + wh) <= 0.0)
            D = ty - (wy + wh);
        A = (0.0 <= D) ? ty - wy : wh;
        break;
    case GENIE_DIR_RIGHT:
        L0 = ty - wy;
        R0 = (th + ty) - (wy + wh);
        K = ww;
        E = tx;
        S0 = wx;
        F = tx + tw;
        if (tx - (wx + ww) <= 0.0)
            D = tx - (wx + ww);
        A = (0.0 <= D) ? tx - wx : ww;
        break;
    case GENIE_DIR_LEFT:
        L0 = ty - wy;
        R0 = (th + ty) - (wy + wh);
        K = -ww;
        E = tw + tx;
        S0 = wx + ww;
        F = tx;
        if (0.0 <= E - wx)
            D = E - wx;
        A = (D <= 0.0) ? E - S0 : -ww;
        break;
    default:
        return -1;
    }
    const int toward_plus = (dir & ~1) == 2;

    double gap = fabs(A - K);
    double m = (fabs(R0) + fabs(L0)) * 0.125;
    if (fabs(D + D) > m)
        m = fabs(D + D);
    double n1 = (double)(int)log(fabs(m) * 0.5 + 1.0) * 50.0;
    double absA = fabs(A);
    double n2 = (double)(int)log(absA * 0.5 + 1.0) * 50.0;
    double x = ((gap * n2) / absA) / n2;
    double a = (acos(1.0 - (x + x)) / genie_pi_d) * n2;
    double shift = (a < n1) ? n1 - a : 0.0;
    double T1 = (n1 <= a) ? a : n1;
    double t = (double)p * (n2 + shift);
    double t1 = (t <= T1) ? t : T1;
    if (t < 0.0)
        t1 = 0.0;
    double t2 = t - shift;
    double u2 = (t2 <= n2) ? t2 : n2;
    if (t2 < 0.0)
        u2 = 0.0;
    u2 /= n2;
    double e1 = cos((t1 / T1) * genie_pi_d) * -0.5 + 0.5;
    double e2 = cos(u2 * genie_pi_d) * -0.5 + 0.5;

    double L = L0 * e1, R = R0 * e1;
    double base = e1 * D + S0;
    int len_i = (int)(A + 0.5);
    double lead = base + A * e2;
    double trail = K + lead;
    double trail_c = trail;
    if (0.0 < e2) {
        int keep = toward_plus ? (trail < E) : (E < trail);
        if (!keep && trail != E)
            trail_c = E;
    }
    double len = (double)len_i;

    float rows[GENIE_MAX_ROWS * 3 + 8];
    int counter = 0;
    double el = genie__ease_profile(lead, base, len);
    rows[0] = (float)lead;
    rows[1] = (float)(L * el);
    rows[2] = (float)(el * R);
    int n = genie__subdivide(lead, trail_c, base, L, R, rows, 3, len_i, &counter, &q);
    double et = genie__ease_profile(trail_c, base, len);
    rows[n + 3] = (float)trail_c;
    rows[n + 4] = (float)(L * et);
    rows[n + 5] = (float)(et * R);
    int head = 3;
    if (gap < fabs(A * e2)) {
        double y = trail;
        if (toward_plus) {
            if (F < trail)
                y = F;
        } else if (trail < F)
            y = F;
        rows[n + 6] = (float)y;
        rows[n + 7] = (float)L;
        rows[n + 8] = (float)R;
        head = 6;
    }
    int nf = head + n + 3;
    int nrows = nf / 3;
    if (nrows > q.max_rows)
        nrows = q.max_rows;

    const float first = rows[0], last = rows[(nrows - 1) * 3];
    genie_vertex *v = out->v;
    if (dir == GENIE_DIR_UP || dir == GENIE_DIR_DOWN) {
        float scale = (dir == GENIE_DIR_DOWN) ? (float)(wh / ((double)last - (double)first)) : (float)(wh / ((double)first - (double)last));
        for (int i = 0; i < nrows; i++) {
            int r = (dir == GENIE_DIR_DOWN) ? i : nrows - 1 - i;
            float my = rows[r * 3], l = rows[r * 3 + 1], rr = rows[r * 3 + 2];
            float tv = (dir == GENIE_DIR_DOWN) ? (float)(((double)my - (double)first) * (double)scale) : (float)(((double)my - (double)last) * (double)scale);
            v[2 * i] = (genie_vertex){0.0f, tv, (float)((double)l + wx), my};
            v[2 * i + 1] = (genie_vertex){(float)ww, tv, (float)((double)rr + ww + wx), my};
        }
        out->cols = 2;
        out->rows = nrows;
    } else {
        float scale = (dir == GENIE_DIR_RIGHT) ? (float)(ww / ((double)last - (double)first)) : (float)(ww / ((double)first - (double)last));
        for (int i = 0; i < nrows; i++) {
            int r = (dir == GENIE_DIR_RIGHT) ? i : nrows - 1 - i;
            float mx = rows[r * 3], l = rows[r * 3 + 1], rr = rows[r * 3 + 2];
            float tu = (dir == GENIE_DIR_RIGHT) ? (float)(((double)mx - (double)first) * (double)scale) : (float)(((double)mx - (double)last) * (double)scale);
            v[i] = (genie_vertex){tu, 0.0f, mx, (float)((double)l + wy)};
            v[nrows + i] = (genie_vertex){tu, (float)wh, mx, (float)((double)rr + wh + wy)};
        }
        out->cols = nrows;
        out->rows = 2;
    }
    return 0;
}

genie_box genie_icon_rect(double w, double h)
{
    genie_box b;
    if (w >= h) {
        double hh = 120.0 * h / w;
        b = (genie_box){4.0f, (float)(64.0 - hh * 0.5), 124.0f, (float)(64.0 + hh * 0.5)};
    } else {
        double ww = 120.0 * w / h;
        b = (genie_box){(float)(64.0 - ww * 0.5), 4.0f, (float)(64.0 + ww * 0.5), 124.0f};
    }
    return b;
}

float genie_tile_inset(float size)
{
    return size > 127.0f ? 3.0f : (size > 47.0f ? 2.0f : 1.0f);
}

genie_box genie_target_rect(genie_box tile, genie_box ic, float in)
{
    const float k = 0.0078125f;
    float w = tile.x1 - tile.x0, h = tile.y1 - tile.y0;
    return (genie_box){in + ic.x0 * w * k + tile.x0, (ic.y0 * h * k + tile.y0) - in, (ic.x1 * w * k + tile.x0) - in, in + h * ic.y1 * k + tile.y0};
}

float genie_progress(double now, double start, double duration, int minimizing)
{
    double u = now - start;
    if (u < 0.0)
        u = 0.0;
    if (u > duration)
        u = duration;
    u /= duration;
    return (float)(minimizing ? u : 1.0 - u);
}

double genie_tile_open(double p)
{
    return 0.5 - 0.5 * cos(genie_pi_d * p);
}

#define GENIE__FLT_MAX 3.4028234663852886e+38
#define GENIE__TWO_PI_F 6.2831854820251465f

typedef struct {
    float x[4], y[4], u[4], v[4];
} genie__cell;

static genie__cell genie__cell_at(const genie_mesh *m, int r, int c)
{
    const genie_vertex *k[4] = {&m->v[r * m->cols + c], &m->v[r * m->cols + c + 1], &m->v[(r + 1) * m->cols + c + 1], &m->v[(r + 1) * m->cols + c]};
    genie__cell q;
    for (int i = 0; i < 4; i++) {
        q.x[i] = k[i]->x;
        q.y[i] = k[i]->y;
        q.u[i] = k[i]->u;
        q.v[i] = k[i]->v;
    }
    return q;
}

static float genie__guard(float a, float b)
{
    float d = a - b;
    if (a == b || fabs((double)d) < 1e-05)
        return (float)GENIE__FLT_MAX;
    return d;
}

static void genie__recontextualize(float *a, float *b)
{
    float f1 = *a;
    if (!(0.0f <= f1 || *b <= 0.0f || (double)(*b - f1) <= genie_pi_d)) {
        *a = f1 + GENIE__TWO_PI_F;
        return;
    }
    if (f1 <= 0.0f)
        return;
    float f2 = *b;
    if (0.0f <= f2)
        return;
    if ((double)(f1 - f2) <= genie_pi_d)
        return;
    *b = f2 + GENIE__TWO_PI_F;
}

static float genie__hyp(float dx, float dy)
{
    return (float)sqrt((double)(dx * dx + dy * dy));
}

int genie_mesh_transform_point(const genie_mesh *m, double u, double v, double *ox, double *oy)
{
    float umin = m->v[0].u, vmin = m->v[0].v, umax = umin, vmax = vmin;
    for (int i = 0; i < m->cols * m->rows; i++) {
        if (m->v[i].u < umin)
            umin = m->v[i].u;
        if (m->v[i].u > umax)
            umax = m->v[i].u;
        if (m->v[i].v < vmin)
            vmin = m->v[i].v;
        if (m->v[i].v > vmax)
            vmax = m->v[i].v;
    }
    double du = 0.0, dv = 0.0;
    if (u < umin) {
        du = u - umin;
        u = umin;
    } else if ((double)umax < u) {
        du = u - umax;
        u = umax;
    }
    if (!((double)vmin <= v && v <= (double)vmax)) {
        double lim = v < vmin ? vmin : vmax;
        dv = v - lim;
        v = lim;
    }
    int found = 0, count = 0, corner = 0;
    double bx = 0, by = 0;
    float scale_sum = 0.0f, angle_sum = 0.0f, corner_angle = 0.0f;
    for (int r = 0; r + 1 < m->rows && !corner; r++)
        for (int c = 0; c + 1 < m->cols && !corner; c++) {
            const genie__cell q = genie__cell_at(m, r, c);
            const float *X = q.x, *Y = q.y, *U = q.u, *V = q.v;
            float mnu = U[0], mxu = U[0], mnv = V[0], mxv = V[0];
            for (int i = 1; i < 4; i++) {
                if (U[i] < mnu)
                    mnu = U[i];
                if (U[i] > mxu)
                    mxu = U[i];
                if (V[i] < mnv)
                    mnv = V[i];
                if (V[i] > mxv)
                    mxv = V[i];
            }
            if (!((double)mnu <= u && u <= (double)mxu && (double)mnv <= v && v <= (double)mxv))
                continue;
            int outside = 0, k = 0, j = 1;
            for (int step = 0; step < 4; step++) {
                if ((double)(U[j] - U[k]) * (v - (double)V[k]) - (double)(V[j] - V[k]) * (u - (double)U[k]) < 0.0) {
                    outside = 1;
                    break;
                }
                j = k ^ 2;
                k = step + 1;
            }
            if (outside)
                continue;
            float cu[2];
            double cx[2], cy[2];
            int n = 0;
            for (int e = 0; e < 4 && n < 2; e++) {
                int e1 = (e + 1) & 3;
                float vk = V[e], vk1 = V[e1];
                float lo = vk < vk1 ? vk : vk1, hi = vk < vk1 ? vk1 : vk;
                if (vk != vk1 && (double)lo <= v && v <= (double)hi) {
                    float den = vk1 - vk;
                    double dd = fabs((double)den) < 1e-05 ? GENIE__FLT_MAX : (double)den;
                    float t = (float)((v - (double)vk) / dd);
                    float w = 1.0f - t;
                    cu[n] = U[e] * w + t * U[e1];
                    cx[n] = (double)(w * X[e] + X[e1] * t);
                    cy[n] = (double)(w * Y[e] + Y[e1] * t);
                    n++;
                }
            }
            if (n == 0)
                continue;
            if (n == 1) {
                cu[1] = cu[0];
                cx[1] = cx[0];
                cy[1] = cy[0];
            }
            float s = (cu[1] == cu[0]) ? 0.5f : (float)((u - (double)cu[0]) / (double)(cu[1] - cu[0]));
            double oms = (double)(1.0f - s);
            bx = cx[0] * oms + cx[1] * (double)s;
            by = cy[0] * oms + cy[1] * (double)s;
            found = 1;

            if (du != 0.0 && dv != 0.0) {
                float wq = (float)((double)(float)atan(fabs(dv / du)) / 1.5707963267948966);
                int both_neg = du < 0.0 && dv < 0.0;
                int mixed_a = !both_neg && du < 0.0 && dv > 0.0;
                int mixed_b = !both_neg && du > 0.0 && dv < 0.0;
                float ah = dv > 0.0 ? atan2f(Y[2] - Y[3], X[2] - X[3]) : atan2f(Y[1] - Y[0], X[1] - X[0]);
                float av = du > 0.0 ? atan2f(Y[2] - Y[1], X[2] - X[1]) : atan2f(Y[3] - Y[0], X[3] - X[0]);
                if (mixed_a || mixed_b) {
                    av = (float)(genie_pi_d - (double)av);
                    ah = -ah;
                }
                genie__recontextualize(&av, &ah);
                float ang = (1.0f - wq) * ah + wq * av;
                if (mixed_a)
                    ang = (float)(genie_pi_d - (double)ang);
                else if (both_neg)
                    ang = (float)((double)ang + -genie_pi_d);
                else if (mixed_b)
                    ang = -ang;
                corner_angle = ang;
                count = 1;
                corner = 1;
                break;
            }
            double t;
            float scale_c, a8, ac, sa, sb_len, sb_den;
            if (dv != 0.0) {
                if (dv <= 0.0) {
                    t = (u - (double)U[1]) / (double)genie__guard(U[0], U[1]);
                    scale_c = genie__hyp(X[0] - X[1], Y[0] - Y[1]) / genie__guard(U[1], U[0]);
                } else {
                    t = (u - (double)U[2]) / (double)genie__guard(U[3], U[2]);
                    scale_c = genie__hyp(X[3] - X[2], Y[3] - Y[2]) / genie__guard(U[2], U[3]);
                }
                a8 = atan2f(Y[3] - Y[0], X[3] - X[0]);
                ac = atan2f(Y[2] - Y[1], X[2] - X[1]);
                sa = genie__hyp(X[0] - X[3], Y[0] - Y[3]) / genie__guard(V[3], V[0]);
                sb_len = genie__hyp(X[1] - X[2], Y[1] - Y[2]);
                sb_den = genie__guard(V[2], V[1]);
            } else {
                if (du <= 0.0) {
                    t = (v - (double)V[3]) / (double)genie__guard(V[0], V[3]);
                    scale_c = genie__hyp(X[0] - X[3], Y[0] - Y[3]) / genie__guard(V[3], V[0]);
                } else {
                    t = (v - (double)V[2]) / (double)genie__guard(V[1], V[2]);
                    scale_c = genie__hyp(X[1] - X[2], Y[1] - Y[2]) / genie__guard(V[2], V[1]);
                }
                a8 = atan2f(Y[1] - Y[0], X[1] - X[0]);
                ac = atan2f(Y[2] - Y[3], X[2] - X[3]);
                sa = genie__hyp(X[0] - X[1], Y[0] - Y[1]) / genie__guard(U[1], U[0]);
                sb_len = genie__hyp(X[3] - X[2], Y[3] - Y[2]);
                sb_den = genie__guard(U[2], U[3]);
            }
            float tf = (float)t;
            genie__recontextualize(&a8, &ac);
            float w = 1.0f - tf;
            a8 = a8 * tf + ac * w;
            genie__recontextualize(&a8, &angle_sum);
            angle_sum = a8 + angle_sum;
            float side = w * (sb_len / sb_den) + sa * tf;
            if (1.0f <= side || 1.0f <= scale_c) {
                if (1.0f <= side) {
                    side = scale_c;
                    if (1.0f <= scale_c)
                        side = 1.0f;
                }
            } else {
                side = side * 0.5f + scale_c * 0.5f;
            }
            count++;
            scale_sum = scale_sum + side;
        }
    if (!found || count == 0)
        return 0;
    if (du == 0.0 && dv == 0.0) {
        *ox = bx;
        *oy = by;
        return 1;
    }
    float scale = scale_sum == 0.0f ? 1.0f : scale_sum / (float)count;
    float mag;
    if (du != 0.0 && dv != 0.0)
        mag = sqrtf((float)(dv * dv + du * du));
    else if (du != 0.0)
        mag = (float)du;
    else
        mag = (float)dv;
    float angle = corner ? corner_angle : angle_sum / (float)count;
    if (scale < 1.0f) {
        float s2 = sqrtf(1.1f * scale + -0.1f);
        float sc = (0.1f < scale) ? s2 : scale;
        if (sc < 0.1f)
            sc = 0.1f;
        mag = mag * sc;
    }
    *ox = bx + (double)(mag * cosf(angle));
    *oy = by + (double)(mag * sinf(angle));
    return 1;
}

int genie_build_shadow_mesh(const genie_mesh *m, genie_rect rect, genie_rect tex, genie_shadow_mesh *out)
{
    const int cols = m->cols, rows = m->rows, oc = cols + 2, orr = rows + 2;
    if (oc * orr > GENIE_MAX_SHADOW_VERTICES)
        return -1;
    const float sx = (float)(tex.w / rect.w), sy = (float)(tex.h / rect.h);
    for (int j = 0; j < orr; j++)
        for (int i = 0; i < oc; i++) {
            const int inner = i >= 1 && i <= cols && j >= 1 && j <= rows;
            const double u = i == 0 ? rect.x : (i == oc - 1 ? rect.x + rect.w : (double)m->v[i - 1].u);
            const double v = j == 0 ? rect.y : (j == orr - 1 ? rect.y + rect.h : (double)m->v[(j - 1) * cols].v);
            genie_vertex *o = &out->v[j * oc + i];
            if (inner) {
                const genie_vertex *src = &m->v[(j - 1) * cols + (i - 1)];
                o->x = src->x;
                o->y = src->y;
            } else {
                double x = 0, y = 0;
                genie_mesh_transform_point(m, u, v, &x, &y);
                o->x = (float)x;
                o->y = (float)y;
            }
            o->u = (i == 0) ? (float)tex.x : (i == oc - 1 ? (float)(tex.w + tex.x) : (float)((double)sx * ((double)(float)u - rect.x) + tex.x));
            o->v = (j == 0) ? (float)tex.y : (j == orr - 1 ? (float)(tex.h + tex.y) : (float)((double)sy * ((double)(float)v - rect.y) + tex.y));
        }
    out->cols = oc;
    out->rows = orr;
    return 0;
}
