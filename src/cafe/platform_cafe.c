#include "platform.h"
#include <SDL2/SDL.h>
#include <stdlib.h>
#include <string.h>

static SDL_Window   *window;
static SDL_Renderer *renderer;
static SDL_Texture  *white;
static PlatInput     input;
static Uint64        last_tick;
static SDL_GameController *pads[MAX_PLAYERS] = { NULL };

struct PlatLayer { SDL_Texture *tex; int w, h; };

bool plat_init(const char *title, int width, int height) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) != 0)
        return false;

    window = SDL_CreateWindow(title, 0, 0, width, height, 0);
    if (!window) return false;

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) return false;

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    white = SDL_CreateTexture(renderer,
        SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STATIC, 1, 1);
    Uint32 px = 0xFFFFFFFF;
    SDL_UpdateTexture(white, NULL, &px, sizeof(px));
    SDL_SetTextureBlendMode(white, SDL_BLENDMODE_BLEND);

    // Open up to MAX_PLAYERS controllers.
    int opened = 0;
    int n = SDL_NumJoysticks();
    for (int i = 0; i < n && opened < MAX_PLAYERS; i++) {
        if (SDL_IsGameController(i)) {
            pads[opened] = SDL_GameControllerOpen(i);
            if (pads[opened]) opened++;
        }
    }

    last_tick = SDL_GetPerformanceCounter();
    return true;
}

void plat_shutdown(void) {
    for (int i = 0; i < MAX_PLAYERS; i++)
        if (pads[i]) SDL_GameControllerClose(pads[i]);
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
        if (e.type == SDL_MOUSEBUTTONDOWN || e.type == SDL_FINGERDOWN) {
            input.mouse_down = true;
            int mx, my;
            SDL_GetMouseState(&mx, &my);
            input.mouse_x = (float)mx;
            input.mouse_y = (float)my;
        }
    }

    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (!pads[i]) continue;

        if (SDL_GameControllerGetButton(pads[i], SDL_CONTROLLER_BUTTON_DPAD_LEFT))
            input.players[i].left = true;
        if (SDL_GameControllerGetButton(pads[i], SDL_CONTROLLER_BUTTON_DPAD_RIGHT))
            input.players[i].right = true;
        if (SDL_GameControllerGetButton(pads[i], SDL_CONTROLLER_BUTTON_DPAD_UP))
            input.players[i].jump = true;
        if (SDL_GameControllerGetButton(pads[i], SDL_CONTROLLER_BUTTON_A))
            input.players[i].jump = true;

        Sint16 ax = SDL_GameControllerGetAxis(pads[i], SDL_CONTROLLER_AXIS_LEFTX);
        if (ax < -8000) input.players[i].left  = true;
        if (ax >  8000) input.players[i].right = true;

        // Pad 0 also fires bombs on right-stick-down.
        if (i == 0) {
            Sint16 ry = SDL_GameControllerGetAxis(pads[i], SDL_CONTROLLER_AXIS_RIGHTY);
            if (ry > 16000) {
                input.mouse_down = true;
                input.mouse_x = 640.0f;
                input.mouse_y = 360.0f;
            }
        }
    }

    // USB keyboard fallback (single player only).
    const Uint8 *ks = SDL_GetKeyboardState(NULL);
    if (ks[SDL_SCANCODE_A]) input.players[0].left  = true;
    if (ks[SDL_SCANCODE_D]) input.players[0].right = true;
    if (ks[SDL_SCANCODE_W]) input.players[0].jump  = true;

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

const char *plat_asset_dir(void) { return "sd:/apps/towerdemo/assets"; }

bool plat_find_map(char *out, size_t out_size) {
    (void)out; (void)out_size;
    return false;
}

const PlatInput *plat_input(void) { return &input; }