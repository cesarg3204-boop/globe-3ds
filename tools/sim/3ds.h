// Minimal desktop stand-in for libctru so the game can be rendered to images for testing.
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
typedef uint8_t u8; typedef uint16_t u16; typedef uint32_t u32; typedef uint64_t u64; typedef int16_t s16;
#define BIT(n) (1U<<(n))
enum { KEY_A=BIT(0), KEY_B=BIT(1), KEY_SELECT=BIT(2), KEY_START=BIT(3), KEY_DRIGHT=BIT(4), KEY_DLEFT=BIT(5),
 KEY_DUP=BIT(6), KEY_DDOWN=BIT(7), KEY_R=BIT(8), KEY_L=BIT(9), KEY_X=BIT(10), KEY_Y=BIT(11), KEY_TOUCH=BIT(20) };
typedef struct { u16 px, py; } touchPosition;
typedef struct { s16 dx, dy; } circlePosition;
typedef enum { GFX_TOP, GFX_BOTTOM } gfxScreen_t;
typedef enum { GFX_LEFT } gfx3dSide_t;
typedef int CFG_Region;
void gfxInitDefault(void); void gfxExit(void);
bool aptMainLoop(void); void hidScanInput(void); u32 hidKeysDown(void); u32 hidKeysHeld(void);
void hidTouchRead(touchPosition*); void hidCircleRead(circlePosition*); u64 osGetTime(void);
