#include "platform.h"
#include "physics.h"
#include "colors.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

typedef struct {
    float x, y;
    float w, h;
    float vx, vy;
    bool  on_ground;
    uint8_t team;
    float fallback_x, fallback_y;
} Player;

static Player players[MAX_PLAYERS];
static World  world;
static int    mouse_owner = 0;

static const int PLAYER_W = 12;
static const int PLAYER_H = 12;
static const int PLAYER_SPEED = 240;
static const int PLAYER_JUMP  = -440;
static const float BOMB_RADIUS = 40.0f;

static void fill_hex(float x, float y, float w, float h, uint32_t hex) {
    plat_fill_rect(x, y, w, h, COL_R(hex), COL_G(hex), COL_B(hex));
}

static void fill_hex_rot(float x, float y, float w, float h, float angle,
                         uint32_t hex) {
    plat_fill_rect_rot(x, y, w, h, angle, COL_R(hex), COL_G(hex), COL_B(hex));
}

static uint32_t cell_color(uint8_t v) {
    switch (v) {
        case CELL_RED:         return COL_RED;
        case CELL_RED_ROOT:    return COL_RED_ROOT;
        case CELL_RED_DOOR:    return COL_RED_DOOR;
        case CELL_YELLOW:      return COL_YELLOW;
        case CELL_YELLOW_ROOT: return COL_YELLOW_ROOT;
        case CELL_YELLOW_DOOR: return COL_YELLOW_DOOR;
        case CELL_BRIDGE:      return COL_BRIDGE;
        case CELL_BRIDGE_DOOR: return COL_BRIDGE_DOOR;
        case CELL_BRIDGE_ROCK: return COL_INDESTRUCTIBLE;
        default:               return COL_DEBRIS;
    }
}

// ------------------------------------------------------------------
// Player
// ------------------------------------------------------------------

// A cell blocks the player unless it's a door or a spawn brick — whether
// it's anchored in the structural grid, still falling, or already landed
// as debris.
static bool cell_blocks_player(int i) {
    uint8_t v = world.cells[i];
    if (v && cell_is_door(v))  v = 0;
    if (v && cell_is_spawn(v)) v = 0;
    if (v) return true;

    if (world.debris[i]) {
        uint8_t d = world.debris_cell[i];
        if (cell_is_door(d) || cell_is_spawn(d)) return false;
        return true;
    }

    return false;
}

static void player_collide(Player *p, int axis) {
    int x0 = (int)(p->x) / CELL;
    int x1 = (int)(p->x + p->w - 1) / CELL;
    int y0 = (int)(p->y) / CELL;
    int y1 = (int)(p->y + p->h - 1) / CELL;

    for (int y = y0; y <= y1; y++) {
        for (int x = x0; x <= x1; x++) {
            if (!world_in_bounds(x, y)) continue;
            int i = world_idx(x, y);
            if (!cell_blocks_player(i)) continue;

            if (axis == 0) {
                if (p->vx > 0)      p->x = x * CELL - p->w;
                else if (p->vx < 0) p->x = (x + 1) * CELL;
                p->vx = 0;
            } else {
                if (p->vy > 0) {
                    p->y = y * CELL - p->h;
                    p->on_ground = true;
                } else if (p->vy < 0) {
                    p->y = (y + 1) * CELL;
                }
                p->vy = 0;
            }
        }
    }
}

static void player_respawn(Player *p) {
    uint8_t want = (p->team == CELL_RED) ? CELL_RED_SPAWN : CELL_YELLOW_SPAWN;

    int found_x[512], found_y[512];
    int n = 0;

    for (int y = 0; y < ROWS && n < 512; y++) {
        for (int x = 0; x < COLS && n < 512; x++) {
            int i = world_idx(x, y);
            if (world.cells[i] == want ||
                (world.debris[i] && world.debris_cell[i] == want)) {
                found_x[n] = x;
                found_y[n] = y;
                n++;
            }
        }
    }

    if (n > 0) {
        int pick = rand() % n;
        p->x = found_x[pick] * CELL + (CELL - PLAYER_W) * 0.5f;
        p->y = found_y[pick] * CELL - PLAYER_H;
    } else {
        p->x = p->fallback_x;
        p->y = p->fallback_y;
    }
    p->vx = p->vy = 0;
    p->on_ground = false;
}

static void player_update(Player *p, float dt, const PlayerInput *in) {
    p->vx = 0;
    if (in->left)  p->vx -= PLAYER_SPEED;
    if (in->right) p->vx += PLAYER_SPEED;

    if (in->jump && p->on_ground) {
        p->vy = PLAYER_JUMP;
        p->on_ground = false;
    }

    p->vy += GRAVITY * dt;
    if (p->vy > MAX_FALL) p->vy = MAX_FALL;

    p->x += p->vx * dt;
    player_collide(p, 0);

    p->y += p->vy * dt;
    p->on_ground = false;
    player_collide(p, 1);

    if (p->y > ROWS * CELL + 100) player_respawn(p);
}

// ------------------------------------------------------------------
// Map parsing
// ------------------------------------------------------------------

static bool parse_map_string(World *w, const char *text) {
    const char *p = text;
    int declared_w = 0, declared_h = 0;
    bool header_seen = false;

    while (*p) {
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '#' || *p == '\n' || *p == '\r' || *p == '\0') {
            while (*p && *p != '\n') p++;
            if (*p == '\n') p++;
            continue;
        }
        if (sscanf(p, "%d %d", &declared_w, &declared_h) != 2) return false;
        if (declared_w != COLS || declared_h != ROWS) return false;
        header_seen = true;
        while (*p && *p != '\n') p++;
        if (*p == '\n') p++;
        break;
    }
    if (!header_seen) return false;

    for (int y = 0; y < ROWS; y++) {
        const char *line_start = p;
        const char *line_end = p;
        while (*line_end && *line_end != '\n' && *line_end != '\r') line_end++;

        size_t len = (size_t)(line_end - line_start);
        if (len != (size_t)COLS) {
            fprintf(stderr,
                "map row %d: length %zu, expected %d\n", y, len, COLS);
            return false;
        }

        for (int x = 0; x < COLS; x++) {
            int i = world_idx(x, y);
            w->cells[i] = CELL_EMPTY;
            switch (line_start[x]) {
                case '.': break;
                case 'R': w->cells[i] = CELL_RED;          break;
                case 'Y': w->cells[i] = CELL_YELLOW;       break;
                case 'N': w->cells[i] = CELL_BRIDGE;       break;
                case 'r': w->cells[i] = CELL_RED_ROOT;     break;
                case 'y': w->cells[i] = CELL_YELLOW_ROOT;  break;
                case 'n': w->cells[i] = CELL_BRIDGE_ROCK;  break;
                case 'x': w->cells[i] = CELL_RED_DOOR;     break;
                case 'z': w->cells[i] = CELL_YELLOW_DOOR;  break;
                case '@': w->cells[i] = CELL_RED_SPAWN;    break;
                case '$': w->cells[i] = CELL_YELLOW_SPAWN; break;
                default:
                    fprintf(stderr,
                        "map row %d col %d: bad char '%c'\n",
                        y, x, line_start[x]);
                    return false;
            }
        }

        p = line_end;
        if (*p == '\r') p++;
        if (*p == '\n') p++;
    }
    return true;
}

static bool map_load(World *w, const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return false;

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0 || sz > 1024 * 1024) { fclose(f); return false; }

    char *buf = malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return false; }

    size_t rd = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[rd] = '\0';

    bool ok = parse_map_string(w, buf);
    free(buf);
    return ok;
}

static const char DEFAULT_MAP[] =
#include "default_map.inc"
;

static void build_arena(World *w) {
    if (!parse_map_string(w, DEFAULT_MAP)) {
        fprintf(stderr, "default map failed to parse — empty world\n");
    }
}

// ------------------------------------------------------------------
// Rendering
// ------------------------------------------------------------------

static void draw_spawn_brick(float px, float py, uint8_t v) {
    float s = (float)CELL;
    fill_hex(px, py, s, s, COL_SPAWN_BASE);
    fill_hex(px + 2, py + 2, s - 4, s - 4,
             cell_team(v) == CELL_RED ? COL_RED : COL_YELLOW);
}

static void draw_world(void) {
    for (int y = 0; y < ROWS; y++) {
        for (int x = 0; x < COLS; x++) {
            int i = world_idx(x, y);
            uint8_t v = world.cells[i];
            if (!v) continue;

            float px = (float)(x * CELL);
            float py = (float)(y * CELL);
            float s  = (float)CELL;

            if (cell_is_spawn(v)) {
                draw_spawn_brick(px, py, v);
            } else {
                fill_hex(px, py, s, s, cell_color(v));
            }
        }
    }

    for (int y = 0; y < ROWS; y++) {
        for (int x = 0; x < COLS; x++) {
            int i = world_idx(x, y);
            if (!world.debris[i]) continue;

            float px = (float)(x * CELL);
            float py = (float)(y * CELL);
            uint8_t v = world.debris_cell[i];

            if (cell_is_spawn(v)) {
                draw_spawn_brick(px, py, v);
            } else {
                fill_hex(px, py, CELL, CELL, cell_color(v));
            }
        }
    }

    for (int i = 0; i < world.island_count; i++) {
        Island *isl = &world.islands[i];
        float c = cosf(isl->angle);
        float s = sinf(isl->angle);
        for (int j = 0; j < isl->n; j++) {
            float lx = isl->bricks[j*2];
            float ly = isl->bricks[j*2 + 1];
            float wx = isl->cx + lx*c - ly*s;
            float wy = isl->cy + lx*s + ly*c;
            uint8_t v = isl->teams[j];
            float sz = (float)CELL;

            if (cell_is_spawn(v)) {
                fill_hex_rot(wx, wy, sz, sz, isl->angle, COL_SPAWN_BASE);
                fill_hex_rot(wx, wy, sz - 4, sz - 4, isl->angle,
                             cell_team(v) == CELL_RED ? COL_RED : COL_YELLOW);
            } else {
                fill_hex_rot(wx, wy, sz, sz, isl->angle, cell_color(v));
            }
        }
    }

    for (int i = 0; i < MAX_PLAYERS; i++) {
        Player *p = &players[i];
        uint32_t outline = (i == mouse_owner)
            ? COL_MOUSE_OWNER_OUTLINE
            : COL_PLAYER_OUTLINE;
        fill_hex(p->x, p->y, p->w, p->h, outline);
        fill_hex(p->x + 2, p->y + 2, p->w - 4, p->h - 4,
                 p->team == CELL_RED ? COL_RED : COL_YELLOW);
    }
}

// ------------------------------------------------------------------
// Game loop
// ------------------------------------------------------------------

static void spawn_player(Player *p, uint8_t team, float fx, float fy) {
    p->w          = PLAYER_W;
    p->h          = PLAYER_H;
    p->vx         = 0;
    p->vy         = 0;
    p->on_ground  = false;
    p->team       = team;
    p->fallback_x = fx;
    p->fallback_y = fy;
    player_respawn(p);
}

void game_init(void) {
    world_init(&world);

#ifdef NOMAP
    build_arena(&world);
#else
    char path[512];
    if (plat_find_map(path, sizeof(path))) {
        if (!map_load(&world, path)) {
            fprintf(stderr, "could not load %s, using default map\n", path);
            build_arena(&world);
        } else {
            fprintf(stderr, "loaded map: %s\n", path);
        }
    } else {
        fprintf(stderr, "no .map file found in assets, using default map\n");
        build_arena(&world);
    }
#endif

    mouse_owner = 0;

    spawn_player(&players[0], CELL_RED,    30 * CELL,  10 * CELL);
    spawn_player(&players[1], CELL_YELLOW, 120 * CELL, 10 * CELL);
    spawn_player(&players[2], CELL_RED,    40 * CELL,  10 * CELL);
    spawn_player(&players[3], CELL_YELLOW, 130 * CELL, 10 * CELL);
}

void game_update(float dt) {
    const PlatInput *in = plat_input();

    if (in->reset) {
        world_free(&world);
        game_init();
        return;
    }

    if (in->mouse_swap) {
        mouse_owner = (mouse_owner == 0) ? 1 : 0;
    }

    for (int i = 0; i < MAX_PLAYERS; i++)
        player_update(&players[i], dt, &in->players[i]);

    physics_update_islands(&world, dt);

    if (in->mouse_down) {
        uint8_t bomber_team = players[mouse_owner].team;
        physics_explode(&world, in->mouse_x, in->mouse_y, BOMB_RADIUS, bomber_team);

        for (int i = 0; i < MAX_PLAYERS; i++) {
            if (players[i].team == bomber_team) continue;
            float px = players[i].x + players[i].w / 2.0f;
            float py = players[i].y + players[i].h / 2.0f;
            float dx = px - in->mouse_x;
            float dy = py - in->mouse_y;
            if (dx*dx + dy*dy < BOMB_RADIUS * BOMB_RADIUS)
                player_respawn(&players[i]);
        }
    }
}

void game_render(void) {
    plat_clear(COL_R(COL_SKY), COL_G(COL_SKY), COL_B(COL_SKY));
    draw_world();
}

int main(void) {
    srand((unsigned)time(NULL));
    if (!plat_init("Tower Demo", COLS * CELL, ROWS * CELL))
        return 1;

    game_init();

    while (plat_poll()) {
        float dt = (float)plat_dt();
        game_update(dt);
        game_render();
        plat_present();
    }

    plat_shutdown();
    return 0;
}