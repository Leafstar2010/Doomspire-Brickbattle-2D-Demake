#ifndef PHYSICS_H
#define PHYSICS_H

#include <stdint.h>
#include <stdbool.h>

#define CELL 8
#define COLS 160
#define ROWS 90

// ---- Cell types -----------------------------------------------------------
// Every brick has one identity. No flags, no roots array, no team lookup —
// just a single enum value per cell.

enum CellType {
    CELL_EMPTY          = 0,
    CELL_RED            = 1,
    CELL_YELLOW         = 2,
    CELL_BRIDGE         = 3,
    CELL_RED_DOOR       = 4,
    CELL_YELLOW_DOOR    = 5,
    CELL_BRIDGE_DOOR    = 6,
    CELL_RED_SPAWN      = 7,
    CELL_YELLOW_SPAWN   = 8,
    CELL_RED_ROOT       = 9,
    CELL_YELLOW_ROOT    = 10,
    CELL_BRIDGE_ROCK    = 11,
};

static inline bool cell_is_door(uint8_t v) {
    return v == CELL_RED_DOOR || v == CELL_YELLOW_DOOR || v == CELL_BRIDGE_DOOR;
}

static inline bool cell_is_spawn(uint8_t v) {
    return v == CELL_RED_SPAWN || v == CELL_YELLOW_SPAWN;
}

static inline bool cell_is_root(uint8_t v) {
    return v == CELL_RED_ROOT || v == CELL_YELLOW_ROOT;
}

static inline bool cell_is_rock(uint8_t v) {
    return v == CELL_BRIDGE_ROCK;
}

static inline uint8_t cell_team(uint8_t v) {
    switch (v) {
        case CELL_RED:
        case CELL_RED_DOOR:
        case CELL_RED_SPAWN:
        case CELL_RED_ROOT:        return CELL_RED;
        case CELL_YELLOW:
        case CELL_YELLOW_DOOR:
        case CELL_YELLOW_SPAWN:
        case CELL_YELLOW_ROOT:     return CELL_YELLOW;
        case CELL_BRIDGE:
        case CELL_BRIDGE_DOOR:
        case CELL_BRIDGE_ROCK:     return CELL_BRIDGE;
        default:                   return CELL_EMPTY;
    }
}

// ---- Physics tuning -------------------------------------------------------

#define GRAVITY    1200.0f
#define MAX_FALL   500.0f
#define MAX_OMEGA  3.0f

#define MAX_ISLANDS   64
#define MAX_ISLAND_N  4096

// Island source, used for the pre-physics support check.
enum {
    ISLAND_SRC_BLAST  = 0,   // direct blast cells — always fall
    ISLAND_SRC_STABLE = 1,   // detached from structural grid
    ISLAND_SRC_DEBRIS = 2,   // detached from debris grid
};

typedef struct {
    float   *bricks;
    uint8_t *teams;   // original cell value per brick
    int      n;
    float    cx, cy;
    float    angle;
    float    vx, vy;
    float    omega;
    int      source;
} Island;

typedef struct {
    uint8_t *cells;        // [COLS*ROWS] cell type, 0 = empty
    uint8_t *debris;       // [COLS*ROWS] 0 or 1
    uint8_t *debris_cell;  // [COLS*ROWS] original cell type for debris
    Island   islands[MAX_ISLANDS];
    int      island_count;
} World;

void  world_init(World *w);
void  world_free(World *w);

int   world_idx(int x, int y);
bool  world_in_bounds(int x, int y);

void  physics_run_ccl(World *w);
void  physics_update_debris_support(World *w);
void  physics_update_islands(World *w, float dt);
void  physics_explode(World *w, float wx, float wy, float radius, uint8_t team);
void  physics_create_island(World *w, const int *cells, int count, int source);

#endif