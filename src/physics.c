#include "physics.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

void world_init(World *w) {
    w->cells       = calloc(COLS * ROWS, 1);
    w->debris      = calloc(COLS * ROWS, 1);
    w->debris_cell = calloc(COLS * ROWS, 1);
    w->island_count = 0;
    for (int i = 0; i < MAX_ISLANDS; i++) {
        w->islands[i].bricks = NULL;
        w->islands[i].teams  = NULL;
        w->islands[i].n      = 0;
        w->islands[i].source = 0;
    }
}

void world_free(World *w) {
    free(w->cells);       w->cells = NULL;
    free(w->debris);      w->debris = NULL;
    free(w->debris_cell); w->debris_cell = NULL;
    for (int i = 0; i < w->island_count; i++) {
        free(w->islands[i].bricks);
        free(w->islands[i].teams);
    }
    w->island_count = 0;
}

int  world_idx(int x, int y)       { return y * COLS + x; }
bool world_in_bounds(int x, int y) { return x >= 0 && x < COLS && y >= 0 && y < ROWS; }

void physics_create_island(World *w, const int *cells, int count, int source) {
    if (w->island_count >= MAX_ISLANDS) return;
    if (count <= 0 || count > MAX_ISLAND_N) return;

    float sx = 0, sy = 0;
    for (int k = 0; k < count; k++) {
        sx += (cells[k*3]     + 0.5f) * CELL;
        sy += (cells[k*3 + 1] + 0.5f) * CELL;
    }
    float cx = sx / count;
    float cy = sy / count;

    Island *isl = &w->islands[w->island_count++];
    isl->bricks = malloc(sizeof(float) * count * 2);
    isl->teams  = malloc(sizeof(uint8_t) * count);
    isl->n      = count;
    isl->cx     = cx;
    isl->cy     = cy;
    isl->angle  = 0.0f;
    isl->vx     = 0.0f;
    isl->vy     = 0.0f;
    isl->omega  = ((float)rand() / RAND_MAX - 0.5f) * 2.0f;
    isl->source = source;

    for (int k = 0; k < count; k++) {
        isl->bricks[k*2]     = (cells[k*3]     + 0.5f) * CELL - cx;
        isl->bricks[k*2 + 1] = (cells[k*3 + 1] + 0.5f) * CELL - cy;
        isl->teams[k]        = (uint8_t)cells[k*3 + 2];
    }
}

static void free_island(Island *isl) {
    free(isl->bricks);
    free(isl->teams);
    isl->bricks = NULL;
    isl->teams  = NULL;
    isl->n      = 0;
}

// 8-connectivity: diagonal neighbors count as connected.
static const int DIRS[8][2] = {
    { 1,  0}, {-1,  0}, { 0,  1}, { 0, -1},
    { 1,  1}, { 1, -1}, {-1,  1}, {-1, -1},
};

void physics_run_ccl(World *w) {
    uint8_t *visited   = calloc(COLS * ROWS, 1);
    uint8_t *processed = calloc(COLS * ROWS, 1);
    int     *stack     = malloc(sizeof(int) * COLS * ROWS * 2);
    int     *cells     = malloc(sizeof(int) * COLS * ROWS * 3);

    int sp = 0;
    for (int y = 0; y < ROWS; y++) {
        for (int x = 0; x < COLS; x++) {
            uint8_t v = w->cells[world_idx(x,y)];
            if (v && cell_is_root(v)) {
                visited[world_idx(x,y)] = 1;
                stack[sp++] = x;
                stack[sp++] = y;
            }
        }
    }

    while (sp > 0) {
        int y = stack[--sp];
        int x = stack[--sp];
        for (int d = 0; d < 8; d++) {
            int nx = x + DIRS[d][0];
            int ny = y + DIRS[d][1];
            if (!world_in_bounds(nx,ny)) continue;
            int i = world_idx(nx,ny);
            if (visited[i] || !w->cells[i]) continue;
            visited[i] = 1;
            stack[sp++] = nx;
            stack[sp++] = ny;
        }
    }

    for (int y = 0; y < ROWS; y++) {
        for (int x = 0; x < COLS; x++) {
            int i = world_idx(x,y);
            if (!w->cells[i] || visited[i] || processed[i]) continue;

            int count = 0;
            sp = 0;
            stack[sp++] = x;
            stack[sp++] = y;
            processed[i] = 1;

            while (sp > 0) {
                int cy = stack[--sp];
                int cx = stack[--sp];
                cells[count*3]     = cx;
                cells[count*3 + 1] = cy;
                cells[count*3 + 2] = w->cells[world_idx(cx,cy)];
                count++;

                for (int d = 0; d < 8; d++) {
                    int nx = cx + DIRS[d][0];
                    int ny = cy + DIRS[d][1];
                    if (!world_in_bounds(nx,ny)) continue;
                    int ni = world_idx(nx,ny);
                    if (!w->cells[ni] || visited[ni] || processed[ni]) continue;
                    processed[ni] = 1;
                    stack[sp++] = nx;
                    stack[sp++] = ny;
                }
            }

            for (int k = 0; k < count; k++) {
                int ci = world_idx(cells[k*3], cells[k*3 + 1]);
                w->cells[ci] = 0;
            }
            physics_create_island(w, cells, count, ISLAND_SRC_STABLE);
        }
    }

    free(visited);
    free(processed);
    free(stack);
    free(cells);
}

void physics_update_debris_support(World *w) {
    uint8_t *supported = calloc(COLS * ROWS, 1);
    uint8_t *processed = calloc(COLS * ROWS, 1);
    int     *stack     = malloc(sizeof(int) * COLS * ROWS * 2);
    int     *cells     = malloc(sizeof(int) * COLS * ROWS * 3);

    // Vertical-only support check: a cell is supported only if the cell
    // directly below it is stable or supported debris.
    for (int y = ROWS - 1; y >= 0; y--) {
        for (int x = 0; x < COLS; x++) {
            int i = world_idx(x,y);
            if (!w->debris[i]) continue;
            if (y >= ROWS - 1) continue;
            int below = world_idx(x, y + 1);
            if (w->cells[below] || (w->debris[below] && supported[below]))
                supported[i] = 1;
        }
    }

    for (int y = 0; y < ROWS; y++) {
        for (int x = 0; x < COLS; x++) {
            int i = world_idx(x,y);
            if (!w->debris[i] || supported[i] || processed[i]) continue;

            int count = 0;
            int sp = 0;
            stack[sp++] = x;
            stack[sp++] = y;
            processed[i] = 1;

            while (sp > 0) {
                int cy = stack[--sp];
                int cx = stack[--sp];
                int ci = world_idx(cx, cy);

                cells[count*3]     = cx;
                cells[count*3 + 1] = cy;
                cells[count*3 + 2] = w->debris_cell[ci];
                count++;

                for (int d = 0; d < 8; d++) {
                    int nx = cx + DIRS[d][0];
                    int ny = cy + DIRS[d][1];
                    if (!world_in_bounds(nx,ny)) continue;
                    int ni = world_idx(nx,ny);
                    if (!w->debris[ni] || supported[ni] || processed[ni]) continue;
                    processed[ni] = 1;
                    stack[sp++] = nx;
                    stack[sp++] = ny;
                }
            }

            for (int k = 0; k < count; k++) {
                int ci = world_idx(cells[k*3], cells[k*3 + 1]);
                w->debris[ci] = 0;
                w->debris_cell[ci] = 0;
            }

            if (count > 0) physics_create_island(w, cells, count, ISLAND_SRC_DEBRIS);
        }
    }

    free(supported);
    free(processed);
    free(stack);
    free(cells);
}

// ---- Pre-physics support check -------------------------------------------
// An island is "resting" if any of its bricks is 8-adjacent to a cell that
// is currently in the structural grid. Not just below — any direction.

static bool island_touches_stable(World *w, const Island *isl) {
    for (int j = 0; j < isl->n; j++) {
        int gx = (int)floorf((isl->cx + isl->bricks[j*2]) / CELL);
        int gy = (int)floorf((isl->cy + isl->bricks[j*2 + 1]) / CELL);
        if (!world_in_bounds(gx, gy)) continue;
        for (int d = 0; d < 8; d++) {
            int nx = gx + DIRS[d][0];
            int ny = gy + DIRS[d][1];
            if (!world_in_bounds(nx, ny)) continue;
            if (w->cells[world_idx(nx, ny)]) return true;
        }
    }
    return false;
}

static void island_snap_back(World *w, const Island *isl) {
    for (int j = 0; j < isl->n; j++) {
        int gx = (int)floorf((isl->cx + isl->bricks[j*2]) / CELL);
        int gy = (int)floorf((isl->cy + isl->bricks[j*2 + 1]) / CELL);
        if (!world_in_bounds(gx, gy)) continue;
        int i = world_idx(gx, gy);
        if (w->cells[i]) continue;

        if (isl->source == ISLAND_SRC_STABLE) {
            w->cells[i] = isl->teams[j];
        } else if (isl->source == ISLAND_SRC_DEBRIS) {
            w->debris[i] = 1;
            w->debris_cell[i] = isl->teams[j];
        }
    }
}

// ---- Island integration --------------------------------------------------

static bool island_lands(World *w, const Island *isl) {
    float c = cosf(isl->angle);
    float s = sinf(isl->angle);
    float half = CELL * 0.5f - 1.0f;

    for (int j = 0; j < isl->n; j++) {
        float lx = isl->bricks[j*2];
        float ly = isl->bricks[j*2 + 1];

        float bcy = isl->cy + lx*s + ly*c;
        int brickGY = (int)floorf(bcy / CELL);

        for (int k = 0; k < 4; k++) {
            float ox = (k & 1) ? half : -half;
            float oy = (k & 2) ? half : -half;
            float bx = lx + ox;
            float by = ly + oy;
            float wx = isl->cx + bx*c - by*s;
            float wy = isl->cy + bx*s + by*c;
            int gx = (int)floorf(wx / CELL);
            int gy = (int)floorf(wy / CELL);
            if (!world_in_bounds(gx,gy)) continue;
            if (gy <= brickGY) continue;
            if (w->cells[world_idx(gx,gy)]) return true;
        }
    }
    return false;
}

static void island_land(World *w, const Island *isl) {
    float c = cosf(isl->angle);
    float s = sinf(isl->angle);

    for (int j = 0; j < isl->n; j++) {
        float lx = isl->bricks[j*2];
        float ly = isl->bricks[j*2 + 1];
        float wx = isl->cx + lx*c - ly*s;
        float wy = isl->cy + lx*s + ly*c;
        int gx = (int)floorf(wx / CELL);
        int gy = (int)floorf(wy / CELL);
        if (!world_in_bounds(gx,gy)) continue;
        int i = world_idx(gx,gy);
        if (w->cells[i]) continue;
        w->debris[i] = 1;
        w->debris_cell[i] = isl->teams[j];
    }
}

void physics_update_islands(World *w, float dt) {
    for (int i = w->island_count - 1; i >= 0; i--) {
        Island *isl = &w->islands[i];

        isl->vy += GRAVITY * dt;
        if (isl->vy > MAX_FALL)      isl->vy = MAX_FALL;
        if (isl->omega >  MAX_OMEGA) isl->omega =  MAX_OMEGA;
        if (isl->omega < -MAX_OMEGA) isl->omega = -MAX_OMEGA;

        isl->cx    += isl->vx * dt;
        isl->cy    += isl->vy * dt;
        isl->angle += isl->omega * dt;

        if (island_lands(w, isl)) {
            island_land(w, isl);
            free_island(isl);
            w->islands[i] = w->islands[--w->island_count];
            continue;
        }

        float minY = 1e9f;
        float c = cosf(isl->angle);
        float s = sinf(isl->angle);
        for (int j = 0; j < isl->n; j++) {
            float lx = isl->bricks[j*2];
            float ly = isl->bricks[j*2 + 1];
            float wy = isl->cy + lx*s + ly*c;
            if (wy < minY) minY = wy;
        }
        if (minY > ROWS * CELL + CELL) {
            free_island(isl);
            w->islands[i] = w->islands[--w->island_count];
        }
    }
}

// ---- Explosion -----------------------------------------------------------

void physics_explode(World *w, float wx, float wy, float radius, uint8_t team) {
    float r2 = radius * radius;
    int cx = (int)floorf(wx / CELL);
    int cy = (int)floorf(wy / CELL);
    int rCells = (int)ceilf(radius / CELL);

    int *blast = malloc(sizeof(int) * (2*rCells + 1) * (2*rCells + 1) * 3);
    int blast_count = 0;

    for (int dy = -rCells; dy <= rCells; dy++) {
        for (int dx = -rCells; dx <= rCells; dx++) {
            int x = cx + dx;
            int y = cy + dy;
            if (!world_in_bounds(x,y)) continue;
            float sx = (x + 0.5f) * CELL;
            float sy = (y + 0.5f) * CELL;
            float ddx = sx - wx;
            float ddy = sy - wy;
            if (ddx*ddx + ddy*ddy > r2) continue;

            int i = world_idx(x,y);
            uint8_t v = w->cells[i];
            if (v) {
                if (cell_team(v) == team) continue;
                if (cell_is_rock(v)) continue;
                blast[blast_count*3]     = x;
                blast[blast_count*3 + 1] = y;
                blast[blast_count*3 + 2] = v;
                blast_count++;
                w->cells[i] = 0;
            } else if (w->debris[i]) {
                w->debris[i] = 0;
                w->debris_cell[i] = 0;
            }
        }
    }

    if (blast_count > 0)
        physics_create_island(w, blast, blast_count, ISLAND_SRC_BLAST);
    free(blast);

    physics_run_ccl(w);
    physics_update_debris_support(w);

    // Snap back islands that are resting on anything structural. Loops
    // until no more can be attached — each pass pulls at least one island
    // back into the grid, which may make others eligible on the next pass.
    // Without the loop, stacked islands can't see each other (they're all
    // removed from cells[] at once) and the upper ones falsely detach.
    bool changed = true;
    while (changed) {
        changed = false;
        for (int i = w->island_count - 1; i >= 0; i--) {
            Island *isl = &w->islands[i];
            if (isl->source == ISLAND_SRC_BLAST) continue;
            if (!island_touches_stable(w, isl)) continue;

            island_snap_back(w, isl);
            free_island(isl);
            w->islands[i] = w->islands[--w->island_count];
            changed = true;
        }
    }
}