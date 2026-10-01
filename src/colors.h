#ifndef COLORS_H
#define COLORS_H

#include <stdint.h>

// All colors are 0xRRGGBB.
#define COL_SKY              0x87CEEB

#define COL_RED              0xF54139
#define COL_RED_ROOT         0x791108
#define COL_RED_DOOR         0xC6362F

#define COL_YELLOW           0xFFEB2F
#define COL_YELLOW_ROOT      0x8E730B
#define COL_YELLOW_DOOR      0xCABA24

#define COL_BRIDGE           0x8f8f8f
#define COL_BRIDGE_ROOT      0x555555
#define COL_BRIDGE_DOOR      0x5D5D5D

#define COL_INDESTRUCTIBLE   0xa0a0a0
#define COL_DEBRIS           0x666666

#define COL_SPAWN_BASE       0xB500FF

#define COL_PLAYER_OUTLINE        0xFFFFFF
#define COL_MOUSE_OWNER_OUTLINE   0x00FFFF

#define COL_R(c) ((uint8_t)(((c) >> 16) & 0xFF))
#define COL_G(c) ((uint8_t)(((c) >> 8)  & 0xFF))
#define COL_B(c) ((uint8_t)( (c)        & 0xFF))

#endif