#ifndef GENIE_H
#define GENIE_H

#ifdef __cplusplus
extern "C" {
#endif

enum {
    GENIE_DIR_UP = 1,
    GENIE_DIR_DOWN = 2,
    GENIE_DIR_RIGHT = 3,
    GENIE_DIR_LEFT = 4,
};

#define GENIE_MAX_ROWS 400
#define GENIE_MAX_VERTICES (2 * GENIE_MAX_ROWS)
#define GENIE_DURATION 0.5

typedef struct {
    double x, y, w, h;
} genie_rect;
typedef struct {
    float x0, y0, x1, y1;
} genie_box;
typedef struct {
    float u, v, x, y;
} genie_vertex;

typedef struct {
    int cols, rows;
    genie_vertex v[GENIE_MAX_VERTICES];
} genie_mesh;

int genie_build_mesh(float p, genie_rect window, genie_box target, int direction, genie_mesh *out);

genie_box genie_icon_rect(double window_w, double window_h);

float genie_tile_inset(float tile_size);

genie_box genie_target_rect(genie_box tile, genie_box icon_rect, float tile_inset);

#define GENIE_MAX_SHADOW_VERTICES ((GENIE_MAX_ROWS + 2) * 4)
typedef struct {
    int cols, rows;
    genie_vertex v[GENIE_MAX_SHADOW_VERTICES];
} genie_shadow_mesh;
int genie_build_shadow_mesh(const genie_mesh *window, genie_rect rect, genie_rect tex, genie_shadow_mesh *out);

int genie_mesh_transform_point(const genie_mesh *m, double u, double v, double *x, double *y);

float genie_progress(double now, double start, double duration, int minimizing);

double genie_tile_open(double p);

#ifdef __cplusplus
}
#endif
#endif
