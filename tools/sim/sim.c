// Replays a script of inputs and logs draw commands for frames marked "dump" (rendered by render.py).
#include "citro2d.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
static FILE* script; static FILE* out; static u32 down, held; static touchPosition tp; static circlePosition cp;
static int frame = 0, dumping = 0; static char dumpName[64];
static C3D_RenderTarget targets[2];
void gfxInitDefault(void) { script = fopen(getenv("SIM_SCRIPT"), "r"); out = fopen(getenv("SIM_OUT"), "w"); }
void gfxExit(void) { fclose(out); }
static u32 prevHeld;
static int waitFrames = 0;
bool aptMainLoop(void) {
	char line[256];
	down = 0; dumping = 0;
	if (waitFrames > 0) { waitFrames--; held = 0; cp.dx = cp.dy = 0; frame++; return true; }
	for (;;) {
		if (!fgets(line, sizeof line, script)) return false;
		if (line[0] == '#' || line[0] == '\n') continue;
		char cmd[32]; int a = 0, b = 0;
		char arg[64] = "";
		sscanf(line, "%31s %63s", cmd, arg);
		if (!strcmp(cmd, "wait")) { waitFrames = atoi(arg) - 1; held = 0; cp.dx = cp.dy = 0; break; }
		if (!strcmp(cmd, "key")) { u32 k = strtoul(arg, 0, 0); down = k; held = k; break; }
		if (!strcmp(cmd, "hold")) { int n; sscanf(line, "%*s %s %d", arg, &n); held = strtoul(arg,0,0); down = 0; frame += n; break; }
		if (!strcmp(cmd, "touch")) { sscanf(line, "%*s %d %d", &a, &b); tp.px = a; tp.py = b; down = KEY_TOUCH; held = KEY_TOUCH; break; }
		if (!strcmp(cmd, "circle")) { sscanf(line, "%*s %d %d", &a, &b); cp.dx = a; cp.dy = b; held = 0; break; }
		if (!strcmp(cmd, "type")) { /* handled by caller script generator */ continue; }
		if (!strcmp(cmd, "dump")) { dumping = 1; snprintf(dumpName, sizeof dumpName, "%s", arg); held = 0; cp.dx = cp.dy = 0; break; }
	}
	frame++;
	return true;
}
void hidScanInput(void) {}
u32 hidKeysDown(void) { return down; } u32 hidKeysHeld(void) { return held; }
void hidTouchRead(touchPosition* t) { *t = tp; } void hidCircleRead(circlePosition* c) { *c = *&cp; }
u64 osGetTime(void) { return 1700000000000ULL + frame * 16ULL; }
bool C3D_Init(size_t s) { return true; } void C3D_Fini(void) {} bool C2D_Init(size_t s) { return true; } void C2D_Fini(void) {} void C2D_Prepare(void) {}
C3D_RenderTarget* C2D_CreateScreenTarget(gfxScreen_t s, gfx3dSide_t d) { targets[s].screen = s; return &targets[s]; }
C2D_TextBuf C2D_TextBufNew(size_t n) { return (C2D_TextBuf)1; } void C2D_TextBufDelete(C2D_TextBuf b) {} void C2D_TextBufClear(C2D_TextBuf b) {}
const char* C2D_TextParse(C2D_Text* t, C2D_TextBuf b, const char* s) { snprintf(t->s, sizeof t->s, "%s", s); return s + strlen(s); }
void C2D_TextOptimize(const C2D_Text* t) {}
void C2D_TextGetDimensions(const C2D_Text* t, float sx, float sy, float* w, float* h) { *w = strlen(t->s) * 13.0f * sx; *h = 30 * sy; }
void C2D_DrawText(const C2D_Text* t, u32 flags, float x, float y, float z, float sx, float sy, ...) {
	va_list ap; va_start(ap, sy); u32 c = (flags & C2D_WithColor) ? va_arg(ap, u32) : 0xFF000000; va_end(ap);
	if (dumping) fprintf(out, "T %.1f %.1f %.2f %u %u %s\n", x, y, sx, c, (flags >> 2) & 3, t->s);
}
bool C3D_FrameBegin(u8 f) { if (dumping) fprintf(out, "FRAME %s\n", dumpName); return true; }
void C3D_FrameEnd(u8 f) { if (dumping) fprintf(out, "END\n"); }
void C2D_TargetClear(C3D_RenderTarget* t, u32 c) { if (dumping) fprintf(out, "C %d %u\n", t->screen, c); }
void C2D_SceneBegin(C3D_RenderTarget* t) { if (dumping) fprintf(out, "S %d\n", t->screen); }
static int tris;
bool C2D_DrawTriangle(float x0,float y0,u32 c0,float x1,float y1,u32 c1,float x2,float y2,u32 c2,float z) {
	if (dumping) fprintf(out, "P %.1f %.1f %.1f %.1f %.1f %.1f %u %u %u\n", x0,y0,x1,y1,x2,y2,c0,c1,c2); return true; }
bool C2D_DrawRectangle(float x,float y,float z,float w,float h,u32 a,u32 b,u32 c,u32 d) {
	if (dumping) fprintf(out, "R %.1f %.1f %.1f %.1f %u\n", x,y,w,h,a); return true; }
bool C2D_DrawEllipse(float x,float y,float z,float w,float h,u32 a,u32 b,u32 c,u32 d) {
	if (dumping) fprintf(out, "E %.1f %.1f %.1f %.1f %u %u\n", x,y,w,h,a,d); return true; }
bool C2D_DrawLine(float x0,float y0,u32 c0,float x1,float y1,u32 c1,float th,float z) {
	if (dumping) fprintf(out, "L %.1f %.1f %.1f %.1f %.1f %u\n", x0,y0,x1,y1,th,c0); return true; }
#undef time
time_t sim_time(time_t* t) { const char* s = getenv("SIM_SEED"); return s ? atol(s) : time(t); }
