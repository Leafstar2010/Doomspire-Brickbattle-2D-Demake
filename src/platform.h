#ifndef PLATFORM_H
#define PLATFORM_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define MAX_PLAYERS 4

bool   plat_init(const char *title, int width, int height);
void   plat_shutdown(void);

bool   plat_poll(void);
void   plat_present(void);
double plat_dt(void);

void plat_clear(uint8_t r, uint8_t g, uint8_t b);
void plat_fill_rect(float x, float y, float w, float h,
                    uint8_t r, uint8_t g, uint8_t b);
void plat_fill_rect_rot(float x, float y, float w, float h, float angle,
                        uint8_t r, uint8_t g, uint8_t b);

typedef struct PlatLayer PlatLayer;
PlatLayer *plat_layer_create(int width, int height);
void       plat_layer_begin(PlatLayer *l);
void       plat_layer_fill_rect(PlatLayer *l, float x, float y, float w, float h,
                                uint8_t r, uint8_t g, uint8_t b);
void       plat_layer_end(PlatLayer *l);
void       plat_layer_draw(PlatLayer *l);
void       plat_layer_destroy(PlatLayer *l);

const char *plat_asset_dir(void);
bool        plat_find_map(char *out, size_t out_size);

typedef struct {
    bool left, right, up, jump;
} PlayerInput;

typedef struct {
    PlayerInput players[MAX_PLAYERS];
    bool  mouse_down;
    float mouse_x, mouse_y;
    bool  mouse_swap;   // fire-once on the rising edge of Q (PC only)
    bool  reset;
    bool  quit;
} PlatInput;

const PlatInput *plat_input(void);

#endif