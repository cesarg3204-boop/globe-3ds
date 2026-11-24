#pragma once
#include "3ds.h"
#define C3D_DEFAULT_CMDBUF_SIZE 0x40000
#define C3D_FRAME_SYNCDRAW 1
typedef struct C3D_RenderTarget { int screen; } C3D_RenderTarget;
typedef struct C2D_TextBuf_s* C2D_TextBuf;
typedef struct { char s[256]; } C2D_Text;
enum { C2D_AtBaseline=BIT(0), C2D_WithColor=BIT(1), C2D_AlignLeft=0, C2D_AlignRight=1<<2, C2D_AlignCenter=2<<2, C2D_WordWrap=BIT(4) };
static inline u32 C2D_Color32(u8 r, u8 g, u8 b, u8 a) { return r | (g<<8) | (b<<16) | ((u32)a<<24); }
bool C3D_Init(size_t); void C3D_Fini(void); bool C2D_Init(size_t); void C2D_Fini(void); void C2D_Prepare(void);
C3D_RenderTarget* C2D_CreateScreenTarget(gfxScreen_t, gfx3dSide_t);
C2D_TextBuf C2D_TextBufNew(size_t); void C2D_TextBufDelete(C2D_TextBuf); void C2D_TextBufClear(C2D_TextBuf);
const char* C2D_TextParse(C2D_Text*, C2D_TextBuf, const char*); void C2D_TextOptimize(const C2D_Text*);
void C2D_TextGetDimensions(const C2D_Text*, float, float, float*, float*);
void C2D_DrawText(const C2D_Text*, u32 flags, float x, float y, float z, float sx, float sy, ...);
bool C3D_FrameBegin(u8); void C3D_FrameEnd(u8);
void C2D_TargetClear(C3D_RenderTarget*, u32); void C2D_SceneBegin(C3D_RenderTarget*);
bool C2D_DrawTriangle(float,float,u32,float,float,u32,float,float,u32,float);
bool C2D_DrawRectangle(float x,float y,float z,float w,float h,u32,u32,u32,u32);
static inline bool C2D_DrawRectSolid(float x,float y,float z,float w,float h,u32 c){return C2D_DrawRectangle(x,y,z,w,h,c,c,c,c);}
bool C2D_DrawEllipse(float x,float y,float z,float w,float h,u32,u32,u32,u32);
static inline bool C2D_DrawCircle(float x,float y,float z,float r,u32 a,u32 b,u32 c,u32 d){return C2D_DrawEllipse(x-r,y-r,z,2*r,2*r,a,b,c,d);}
static inline bool C2D_DrawCircleSolid(float x,float y,float z,float r,u32 c){return C2D_DrawCircle(x,y,z,r,c,c,c,c);}
bool C2D_DrawLine(float x0,float y0,u32 c0,float x1,float y1,u32 c1,float thick,float z);
