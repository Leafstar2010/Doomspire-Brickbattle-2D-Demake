#include "platform.h"
#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#define WINDOW_SCALE 0.75f

static SDL_Window   *window;
static SDL_Renderer *renderer;
static SDL_Texture  *white;
static PlatInput     input;
static Uint64        last_tick;
static bool          prev_q = false;

struct PlatLayer { SDL_Texture *tex; int w, h; };

bool plat_init(const char *title, int width, int height) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) return false;

    window = SDL_CreateWindow(title,
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        (int)(width * WINDOW_SCALE),
        (int)(height * WINDOW_SCALE),
        0);
    if (!window) return false;

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) return false;

    SDL_RenderSetLogicalSize(renderer, width, height);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    white = SDL_CreateTexture(renderer,
        SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STATIC, 1, 1);
    Uint32 px = 0xFFFFFFFF;
    SDL_UpdateTexture(white, NULL, &px, sizeof(px));
    SDL_SetTextureBlendMode(white, SDL_BLENDMODE_BLEND);

    last_tick = SDL_GetPerformanceCounter();
    return true;
}

void plat_shutdown(void) {
    if (white)    SDL_DestroyTexture(white);
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window)   SDL_DestroyWindow(window);
    SDL_Quit();
}

bool plat_poll(void) {
    memset(&input, 0, sizeof(input));

    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) input.quit = true;
        if (e.type == SDL_MOUSEBUTTONDOWN) {
            input.mouse_down = true;
            int mx, my;
            SDL_GetMouseState(&mx, &my);
            input.mouse_x = (float)mx / WINDOW_SCALE;
            input.mouse_y = (float)my / WINDOW_SCALE;
        }
    }

    const Uint8 *ks = SDL_GetKeyboardState(NULL);

    if (ks[SDL_SCANCODE_A]) input.players[0].left  = true;
    if (ks[SDL_SCANCODE_D]) input.players[0].right = true;
    if (ks[SDL_SCANCODE_W]) input.players[0].jump  = true;

    if (ks[SDL_SCANCODE_LEFT])  input.players[1].left  = true;
    if (ks[SDL_SCANCODE_RIGHT]) input.players[1].right = true;
    if (ks[SDL_SCANCODE_UP])    input.players[1].jump  = true;

    if (ks[SDL_SCANCODE_F]) input.players[2].left  = true;
    if (ks[SDL_SCANCODE_H]) input.players[2].right = true;
    if (ks[SDL_SCANCODE_T]) input.players[2].jump  = true;

    if (ks[SDL_SCANCODE_J]) input.players[3].left  = true;
    if (ks[SDL_SCANCODE_L]) input.players[3].right = true;
    if (ks[SDL_SCANCODE_I]) input.players[3].jump  = true;

    bool q_now = ks[SDL_SCANCODE_Q] != 0;
    if (q_now && !prev_q) input.mouse_swap = true;
    prev_q = q_now;

    if (ks[SDL_SCANCODE_R]) input.reset = true;

    return !input.quit;
}

void plat_present(void) { SDL_RenderPresent(renderer); }

double plat_dt(void) {
    Uint64 now = SDL_GetPerformanceCounter();
    double dt = (double)(now - last_tick) / SDL_GetPerformanceFrequency();
    last_tick = now;
    if (dt > 0.05) dt = 0.05;
    return dt;
}

void plat_clear(uint8_t r, uint8_t g, uint8_t b) {
    SDL_SetRenderDrawColor(renderer, r, g, b, 255);
    SDL_RenderClear(renderer);
}

void plat_fill_rect(float x, float y, float w, float h,
                    uint8_t r, uint8_t g, uint8_t b) {
    SDL_SetRenderDrawColor(renderer, r, g, b, 255);
    SDL_Rect dst = { (int)x, (int)y, (int)w, (int)h };
    SDL_RenderFillRect(renderer, &dst);
}

void plat_fill_rect_rot(float x, float y, float w, float h, float angle,
                        uint8_t r, uint8_t g, uint8_t b) {
    SDL_SetTextureColorMod(white, r, g, b);
    SDL_Rect dst = { (int)(x - w/2), (int)(y - h/2), (int)w, (int)h };
    SDL_Point ctr = { (int)(w/2), (int)(h/2) };
    SDL_RenderCopyEx(renderer, white, NULL, &dst,
                     angle * 180.0 / 3.14159265358979, &ctr, SDL_FLIP_NONE);
}

PlatLayer *plat_layer_create(int width, int height) { (void)width; (void)height; return NULL; }
void plat_layer_begin(PlatLayer *l) { (void)l; }
void plat_layer_fill_rect(PlatLayer *l, float x, float y, float w, float h,
                          uint8_t r, uint8_t g, uint8_t b) {
    (void)l; plat_fill_rect(x, y, w, h, r, g, b);
}
void plat_layer_end(PlatLayer *l)     { (void)l; }
void plat_layer_draw(PlatLayer *l)    { (void)l; }
void plat_layer_destroy(PlatLayer *l) { (void)l; }

// Assets live next to the executable: <exe_dir>/assets
const char *plat_asset_dir(void) {
    static char dir[MAX_PATH] = {0};
    if (dir[0] == 0) {
        char exe[MAX_PATH];
        DWORD n = GetModuleFileNameA(NULL, exe, MAX_PATH);
        if (n > 0 && n < MAX_PATH) {
            char *slash = strrchr(exe, '\\');
            if (!slash) slash = strrchr(exe, '/');
            if (slash) *slash = '\0';
            snprintf(dir, sizeof(dir), "%s\\assets", exe);
        } else {
            snprintf(dir, sizeof(dir), "assets");
        }
    }
    return dir;
}

bool plat_find_map(char *out, size_t out_size) {
    char pattern[MAX_PATH];
    snprintf(pattern, sizeof(pattern), "%s\\*.map", plat_asset_dir());

    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) return false;

    bool found = false;
    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            snprintf(out, out_size, "%s\\%s", plat_asset_dir(), fd.cFileName);
            found = true;
            break;
        }
    } while (FindNextFileA(h, &fd));

    FindClose(h);
    return found;
}

const PlatInput *plat_input(void) { return &input; }