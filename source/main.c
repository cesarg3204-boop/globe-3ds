// Globe3DS - country guessing games on a 3DS globe.
// Classic: find the hidden country by distance. Bridge: connect two countries through their neighbours.
#include <3ds.h>
#include <citro2d.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <time.h>

#include "countrydata.h"
#include "game.h"
#include "globe.h"

#define TOP_W 400
#define BOT_W 320
#define SCR_H 240

// ---------------------------------------------------------------- palette
#define RGBA(r, g, b, a) C2D_Color32(r, g, b, a)
static u32 CLR_SPACE, CLR_OCEAN_A, CLR_OCEAN_B, CLR_GLOW, CLR_BG, CLR_PANEL, CLR_KEY, CLR_KEY_DOWN,
	CLR_TEXT, CLR_DIM, CLR_ACCENT, CLR_FOUND, CLR_WHITE, CLR_START, CLR_END, CLR_ON, CLR_NEAR, CLR_OFF,
	CLR_REVEAL;
static u32 LAND[8];

static void palette_init(void)
{
	CLR_SPACE = RGBA(6, 9, 20, 255);
	CLR_OCEAN_A = RGBA(28, 62, 110, 255);
	CLR_OCEAN_B = RGBA(10, 26, 56, 255);
	CLR_GLOW = RGBA(90, 150, 255, 40);
	CLR_BG = RGBA(16, 20, 30, 255);
	CLR_PANEL = RGBA(29, 35, 50, 255);
	CLR_KEY = RGBA(43, 51, 70, 255);
	CLR_KEY_DOWN = RGBA(255, 138, 61, 255);
	CLR_TEXT = RGBA(232, 236, 244, 255);
	CLR_DIM = RGBA(140, 150, 170, 255);
	CLR_ACCENT = RGBA(255, 138, 61, 255);
	CLR_FOUND = RGBA(54, 212, 107, 255);
	CLR_WHITE = RGBA(255, 255, 255, 255);
	CLR_START = RGBA(80, 190, 255, 255);
	CLR_END = RGBA(240, 90, 200, 255);
	CLR_ON = RGBA(54, 212, 107, 255);
	CLR_NEAR = RGBA(245, 200, 60, 255);
	CLR_OFF = RGBA(205, 80, 72, 255);
	CLR_REVEAL = RGBA(230, 232, 240, 255);
	// muted land tones for countries not guessed yet
	LAND[0] = RGBA(120, 126, 112, 255);
	LAND[1] = RGBA(126, 132, 116, 255);
	LAND[2] = RGBA(112, 120, 108, 255);
	LAND[3] = RGBA(132, 128, 112, 255);
	LAND[4] = RGBA(116, 124, 118, 255);
	LAND[5] = RGBA(128, 122, 108, 255);
	LAND[6] = RGBA(108, 116, 112, 255);
	LAND[7] = RGBA(124, 130, 120, 255);
}

// Heat scale: touching = deep red, near = orange/yellow, far = icy blue.
static u32 heat_color(int km)
{
	static const float stops[][3] = {
		{200, 16, 32}, {255, 64, 32}, {255, 140, 40}, {255, 210, 90}, {170, 220, 235}, {70, 130, 210},
	};
	if (km <= 0) return RGBA(200, 16, 32, 255);
	float t = sqrtf((km > 16000 ? 16000 : km) / 16000.0f) * 5.0f;
	int i = (int)t;
	if (i >= 5) i = 4;
	float f = t - i;
	if (f > 1) f = 1;
	return RGBA((u8)(stops[i][0] + (stops[i + 1][0] - stops[i][0]) * f),
	            (u8)(stops[i][1] + (stops[i + 1][1] - stops[i][1]) * f),
	            (u8)(stops[i][2] + (stops[i + 1][2] - stops[i][2]) * f), 255);
}

// ---------------------------------------------------------------- text helpers
static C2D_TextBuf textBuf;

static void text(float x, float y, float scale, u32 color, u32 flags, const char* fmt, ...)
{
	char buf[160];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	C2D_Text t;
	C2D_TextParse(&t, textBuf, buf);
	C2D_TextOptimize(&t);
	C2D_DrawText(&t, C2D_WithColor | flags, x, y, 0.9f, scale, scale, color);
}

static float text_width(float scale, const char* s)
{
	C2D_Text t;
	float w, h;
	C2D_TextParse(&t, textBuf, s);
	C2D_TextGetDimensions(&t, scale, scale, &w, &h);
	return w;
}

// draws text shrunk (down to 60%) so it fits in maxW pixels
static void text_fit(float x, float y, float scale, float maxW, u32 color, u32 flags, const char* s)
{
	float w = text_width(scale, s);
	if (w > maxW) scale = scale * maxW / w > scale * 0.6f ? scale * maxW / w : scale * 0.6f;
	text(x, y, scale, color, flags, "%s", s);
}

static void fmt_km(char* out, int km)
{
	if (km >= 1000) sprintf(out, "%d,%03d km", km / 1000, km % 1000);
	else sprintf(out, "%d km", km);
}

// ---------------------------------------------------------------- UI state
typedef enum { VIEW_KEYBOARD, VIEW_LIST } BottomView;

static Game game;
static Stats stats;
static bool inMenu = true;
static int menuSel = 0;
static const char* MODE_NAMES[MODE_COUNT] = { "Classic", "Bridge" };
static const char* MODE_DESC[MODE_COUNT] = {
	"Find the hidden country by how far each guess is.",
	"Link two countries through bordering countries.",
};
static GlobeView view;
static BottomView bottomView = VIEW_KEYBOARD;
static u32 countryColor[512];
static unsigned char markerMask[512];
static int listScroll = 0;
static char toast[64];
static int toastTimer = 0;
static bool confirmGiveUp = false;
static int pressedKey = -1, pressedTimer = 0;
static float idleTimer = 0;

typedef struct { float x, y, w, h; char ch; const char* label; } Key;
#define KEY_DEL 1
#define KEY_ENTER 2
#define KEY_SPACE ' '
static Key keys[40];
static int keyCount = 0;

static void keyboard_init(void)
{
	const char* rows[3] = { "QWERTYUIOP", "ASDFGHJKL", "ZXCVBNM" };
	const float ky[4] = { 112, 144, 176, 208 };
	const float kw = 32, kh = 30;
	for (int r = 0; r < 3; r++) {
		float off = r == 0 ? 0 : 16;
		for (int i = 0; rows[r][i]; i++)
			keys[keyCount++] = (Key){ off + i * kw + 1, ky[r], kw - 2, kh, rows[r][i], NULL };
	}
	keys[keyCount++] = (Key){ 16 + 7 * kw + 1, ky[2], 3 * kw - 2, kh, KEY_DEL, "Del" };
	keys[keyCount++] = (Key){ 1, ky[3], 200 - 2, kh, KEY_SPACE, "space" };
	keys[keyCount++] = (Key){ 200 + 1, ky[3], 120 - 2, kh, KEY_ENTER, "Guess" };
}

static void show_toast(const char* msg)
{
	snprintf(toast, sizeof(toast), "%s", msg);
	toastTimer = 120;
}

static void refresh_colors(void)
{
	for (int i = 0; i < COUNTRY_COUNT; i++) {
		countryColor[i] = LAND[COUNTRIES[i].shade & 7];
		markerMask[i] = 0;
	}
	if (inMenu) return;
	if (game.mode == MODE_BRIDGE) {
		static const u32* routeColor[3] = { &CLR_ON, &CLR_NEAR, &CLR_OFF };
		if (game.gaveUp)
			for (int i = 0; i < game.chainLen; i++) { countryColor[game.chain[i]] = CLR_REVEAL; markerMask[game.chain[i]] = 1; }
		for (int i = 0; i < game.guessCount; i++) {
			int c = game.guesses[i];
			countryColor[c] = *routeColor[bridge_route_status(&game, c)];
			markerMask[c] = 1;
		}
		countryColor[game.start] = CLR_START;
		countryColor[game.end] = CLR_END;
		markerMask[game.start] = markerMask[game.end] = 1;
		return;
	}
	for (int i = 0; i < game.guessCount; i++) {
		int c = game.guesses[i];
		countryColor[c] = c == game.target ? CLR_FOUND : heat_color(country_distance(c, game.target));
		markerMask[c] = 1;
	}
	if (game.gaveUp) {
		countryColor[game.target] = CLR_FOUND;
		markerMask[game.target] = 1;
	}
}

static void focus_between(int a, int b)
{
	const float D = 3.14159265f / 180.0f;
	float x = 0, y = 0, z = 0;
	int ends[2] = { a, b };
	for (int i = 0; i < 2; i++) {
		float la = COUNTRIES[ends[i]].lat * D, lo = COUNTRIES[ends[i]].lon * D;
		x += cosf(la) * cosf(lo); y += cosf(la) * sinf(lo); z += sinf(la);
	}
	globe_focus(&view, atan2f(z, sqrtf(x * x + y * y)) / D, atan2f(y, x) / D);
}

// zoom so that both ends of a bridge fit on the top screen
static void zoom_to_fit(int a, int b)
{
	const float D = 3.14159265f / 180.0f;
	float la1 = COUNTRIES[a].lat * D, lo1 = COUNTRIES[a].lon * D, la2 = COUNTRIES[b].lat * D, lo2 = COUNTRIES[b].lon * D;
	float dot = sinf(la1) * sinf(la2) + cosf(la1) * cosf(la2) * cosf(lo1 - lo2);
	float half = acosf(dot > 1 ? 1 : (dot < -1 ? -1 : dot)) / 2;
	half += (COUNTRIES[a].extent + COUNTRIES[b].extent) * 0.25f * D;  // leave room for the countries themselves
	float z = half > 0.01f ? 68.0f / (GLOBE_RADIUS * sinf(half < 1.5f ? half : 1.5f)) : 4.0f;
	view.zoom = z < 1.0f ? 1.0f : (z > 4.0f ? 4.0f : z);
}

static void new_game(GameMode mode)
{
	inMenu = false;
	game_new(&game, mode);
	stats_load(&stats, mode);
	bottomView = VIEW_KEYBOARD;
	listScroll = 0;
	confirmGiveUp = false;
	view.animating = false;
	if (mode == MODE_BRIDGE) {
		focus_between(game.start, game.end);
		view.lat = view.tlat; view.lon = view.tlon; view.animating = false;  // jump straight there
		zoom_to_fit(game.start, game.end);
	}
	refresh_colors();
}

static void open_menu(void)
{
	inMenu = true;
	bottomView = VIEW_KEYBOARD;
	confirmGiveUp = false;
	refresh_colors();
}

static void finish_game(bool won)
{
	stats.played++;
	if (won) {
		stats.wins++;
		stats.totalGuesses += game.guessCount;
		if (stats.best == 0 || game.guessCount < stats.best) stats.best = game.guessCount;
		stats.streak++;
		if (stats.streak > stats.bestStreak) stats.bestStreak = stats.streak;
	} else {
		stats.streak = 0;
	}
	stats_save(&stats, game.mode);
}

static void submit_guess(int c)
{
	if (c < 0) {
		show_toast(game.input[0] ? "No country matches that" : "Type a country name first");
		return;
	}
	GuessResult r = game_guess(&game, c);
	if (r == RESULT_REPEAT) { show_toast("Already guessed"); return; }
	if (r == RESULT_ENDPOINT) { show_toast("That's one end of the bridge"); return; }
	if (r == RESULT_NONE) return;
	refresh_colors();
	globe_focus(&view, COUNTRIES[c].lat, COUNTRIES[c].lon);
	confirmGiveUp = false;
	if (r == RESULT_WIN) finish_game(true);
	if (r == RESULT_LOSE) finish_game(false);
	if (game.mode == MODE_BRIDGE && (r == RESULT_WIN || r == RESULT_LOSE)) focus_between(game.start, game.end);
}

static void type_char(char ch)
{
	int n = strlen(game.input);
	if (ch == KEY_DEL) {
		if (n > 0) game.input[n - 1] = 0;
	} else if (n < MAX_INPUT) {
		if (ch == ' ' && n == 0) return;
		game.input[n] = (n == 0 || game.input[n - 1] == ' ') ? ch : (ch | 0x20);
		game.input[n + 1] = 0;
	}
	game_update_suggestions(&game);
}

// sorting key for the guess list: classic = distance, bridge = route status (stable)
static int guess_rank(int c)
{
	return game.mode == MODE_BRIDGE ? (int)bridge_route_status(&game, c) : country_distance(c, game.target);
}

// sorted copy of guesses, best first
static int sorted_guesses(int* out)
{
	int n = game.guessCount;
	for (int i = 0; i < n; i++) out[i] = game.guesses[i];
	for (int i = 1; i < n; i++) {
		int v = out[i], dv = guess_rank(v), j = i - 1;
		while (j >= 0 && guess_rank(out[j]) > dv) { out[j + 1] = out[j]; j--; }
		out[j + 1] = v;
	}
	return n;
}

static const char* route_label(RouteStatus r)
{
	return r == ROUTE_ON ? "On a shortest route" : (r == ROUTE_NEAR ? "Close to a route" : "Off route");
}

static void chain_text(char* out, int size)
{
	int len = 0;
	out[0] = 0;
	for (int i = 0; i < game.chainLen && len < size - 1; i++)
		len += snprintf(out + len, size - len, "%s%s", i ? " > " : "", COUNTRIES[game.chain[i]].name);
}

static bool in_rect(touchPosition t, float x, float y, float w, float h)
{
	return t.px >= x && t.px < x + w && t.py >= y && t.py < y + h;
}

// ---------------------------------------------------------------- top screen
static float starX[90], starY[90];
static u8 starA[90];

static void draw_toast(float cx)
{
	if (toastTimer <= 0) return;
	float w = text_width(0.5f, toast) + 20;
	C2D_DrawRectSolid(cx - w / 2, 24, 0.95f, w, 22, RGBA(255, 138, 61, 230));
	text(cx, 27, 0.5f, RGBA(20, 20, 20, 255), C2D_AlignCenter, "%s", toast);
}

static void legend_swatch(float y, u32 color, const char* label)
{
	C2D_DrawRectSolid(TOP_W - 16, y + 2, 0.8f, 8, 8, color);
	text(TOP_W - 20, y - 1, 0.38f, CLR_DIM, C2D_AlignRight, "%s", label);
}

static void draw_top_bridge(float cx)
{
	char buf[200];
	text(8, 4, 0.55f, CLR_ACCENT, 0, "BRIDGE");
	int left = game.maxGuesses - game.guessCount;
	text(TOP_W - 8, 4, 0.5f, left <= 1 && !game.won ? CLR_OFF : CLR_DIM, C2D_AlignRight, "Guesses left: %d", left);

	legend_swatch(40, CLR_START, "start");
	legend_swatch(56, CLR_END, "goal");
	legend_swatch(76, CLR_ON, "on route");
	legend_swatch(92, CLR_NEAR, "close");
	legend_swatch(108, CLR_OFF, "off route");
	if (game.gaveUp) legend_swatch(124, CLR_REVEAL, "solution");

	C2D_DrawRectSolid(0, SCR_H - 34, 0.85f, TOP_W, 34, RGBA(0, 0, 0, 150));
	if (game.won || game.gaveUp) {
		if (game.won)
			text(cx, SCR_H - 32, 0.55f, CLR_ON, C2D_AlignCenter, "Bridge built in %d guess%s!", game.guessCount,
			     game.guessCount == 1 ? "" : "es");
		else
			text(cx, SCR_H - 32, 0.55f, CLR_OFF, C2D_AlignCenter, "%s One route was:",
			     game.guessCount >= game.maxGuesses ? "Out of guesses." : "You gave up.");
		chain_text(buf, sizeof(buf));
		text_fit(cx, SCR_H - 15, 0.42f, TOP_W - 12, CLR_TEXT, C2D_AlignCenter, buf);
	} else if (game.guessCount == 0) {
		snprintf(buf, sizeof(buf), "Connect %s to %s", COUNTRIES[game.start].name, COUNTRIES[game.end].name);
		text_fit(cx, SCR_H - 32, 0.52f, TOP_W - 12, CLR_TEXT, C2D_AlignCenter, buf);
		text(cx, SCR_H - 15, 0.42f, CLR_DIM, C2D_AlignCenter, "using countries that share borders. Shortest bridge: %d countries.",
		     game.minLinks);
	} else {
		int last = game.guesses[game.guessCount - 1];
		RouteStatus r = bridge_route_status(&game, last);
		u32 col = r == ROUTE_ON ? CLR_ON : (r == ROUTE_NEAR ? CLR_NEAR : CLR_OFF);
		text(8, SCR_H - 32, 0.52f, col, 0, "%s", COUNTRIES[last].name);
		text(TOP_W - 8, SCR_H - 32, 0.52f, col, C2D_AlignRight, "%s", route_label(r));
		snprintf(buf, sizeof(buf), "%s  >  ...  >  %s", COUNTRIES[game.start].name, COUNTRIES[game.end].name);
		text_fit(8, SCR_H - 15, 0.42f, TOP_W - 16, CLR_DIM, 0, buf);
	}
	draw_toast(cx);
}

static void draw_top(float time)
{
	for (int i = 0; i < 90; i++)
		C2D_DrawRectSolid(starX[i], starY[i], 0.1f, 1, 1, RGBA(255, 255, 255, starA[i]));

	float cx = TOP_W / 2, cy = 120;
	float R = GLOBE_RADIUS * view.zoom;
	C2D_DrawCircleSolid(cx, cy, 0.2f, R + 7, RGBA(80, 140, 255, 18));
	C2D_DrawCircleSolid(cx, cy, 0.2f, R + 3, CLR_GLOW);
	C2D_DrawCircle(cx, cy, 0.3f, R, CLR_OCEAN_A, CLR_OCEAN_B, CLR_OCEAN_B, CLR_OCEAN_B);
	globe_draw(&view, countryColor, cx, cy);
	int hl = game.guessCount > 0 ? game.guesses[game.guessCount - 1] : -1;
	if (game.won || game.gaveUp) hl = game.target;
	if (game.mode == MODE_BRIDGE && game.guessCount == 0) hl = game.end;
	if (inMenu) hl = -1;
	globe_draw_markers(&view, countryColor, markerMask, hl, time, cx, cy);

	if (inMenu) {
		C2D_DrawRectSolid(0, SCR_H - 46, 0.85f, TOP_W, 46, RGBA(0, 0, 0, 150));
		text(cx, SCR_H - 46, 0.9f, CLR_ACCENT, C2D_AlignCenter, "GLOBE3DS");
		text(cx, SCR_H - 18, 0.42f, CLR_DIM, C2D_AlignCenter, "Pick a game on the touch screen");
		return;
	}

	if (game.mode == MODE_BRIDGE) {
		draw_top_bridge(cx);
		return;
	}

	// title & counter
	text(8, 4, 0.55f, CLR_ACCENT, 0, "GLOBE3DS");
	text(TOP_W - 8, 4, 0.5f, CLR_DIM, C2D_AlignRight, "Guesses: %d", game.guessCount);

	// heat legend
	for (int i = 0; i < 40; i++) {
		int km = (int)((i / 39.0f) * (i / 39.0f) * 16000);
		C2D_DrawRectSolid(TOP_W - 16, 40 + i * 3.5f, 0.8f, 8, 3.5f, heat_color(km));
	}
	text(TOP_W - 20, 34, 0.38f, CLR_DIM, C2D_AlignRight, "near");
	text(TOP_W - 20, 172, 0.38f, CLR_DIM, C2D_AlignRight, "far");

	// status bar
	C2D_DrawRectSolid(0, SCR_H - 34, 0.85f, TOP_W, 34, RGBA(0, 0, 0, 150));
	char km[24];
	if (game.won) {
		text(cx, SCR_H - 32, 0.6f, CLR_FOUND, C2D_AlignCenter, "You found %s!", COUNTRIES[game.target].name);
		text(cx, SCR_H - 15, 0.42f, CLR_TEXT, C2D_AlignCenter, "%d guesses  -  press A to play again", game.guessCount);
	} else if (game.gaveUp) {
		text(cx, SCR_H - 32, 0.6f, CLR_ACCENT, C2D_AlignCenter, "It was %s", COUNTRIES[game.target].name);
		text(cx, SCR_H - 15, 0.42f, CLR_TEXT, C2D_AlignCenter, "Press A to play again");
	} else if (game.guessCount == 0) {
		text(cx, SCR_H - 32, 0.5f, CLR_TEXT, C2D_AlignCenter, "A country is hidden somewhere on the globe.");
		text(cx, SCR_H - 15, 0.42f, CLR_DIM, C2D_AlignCenter, "Guess one below. Warmer colours mean you're closer.");
	} else {
		int last = game.guesses[game.guessCount - 1];
		fmt_km(km, game.lastDistance);
		const char* trend = game.lastTrend > 0 ? "Warmer!" : (game.lastTrend < 0 ? "Colder" : "");
		if (game.lastDistance == 0 && game.guessCount > 0) trend = "Neighbour!";
		text(8, SCR_H - 32, 0.52f, heat_color(game.lastDistance), 0, "%s  %s", COUNTRIES[last].name, km);
		text(TOP_W - 8, SCR_H - 32, 0.52f, game.lastTrend < 0 ? RGBA(150, 200, 255, 255) : CLR_ACCENT,
		     C2D_AlignRight, "%s", trend);
		fmt_km(km, country_distance(game.closest, game.target));
		text(8, SCR_H - 15, 0.42f, CLR_DIM, 0, "Closest so far: %s (%s)", COUNTRIES[game.closest].name, km);
	}

	draw_toast(cx);
}

// ---------------------------------------------------------------- bottom screen
static void draw_button(float x, float y, float w, float h, u32 bg, u32 fg, float scale, const char* label)
{
	C2D_DrawRectSolid(x, y, 0.5f, w, h, bg);
	text(x + w / 2, y + (h - 30 * scale) / 2, scale, fg, C2D_AlignCenter, "%s", label);
}

static void draw_keyboard_view(void)
{
	// input box
	C2D_DrawRectSolid(4, 4, 0.5f, 246, 26, CLR_PANEL);
	C2D_DrawRectSolid(4, 29, 0.5f, 246, 1, CLR_ACCENT);
	if (game.input[0]) {
		text(10, 8, 0.55f, CLR_TEXT, 0, "%s", game.input);
		float w = text_width(0.55f, game.input);
		if ((osGetTime() / 400) & 1) C2D_DrawRectSolid(11 + w, 9, 0.6f, 2, 17, CLR_ACCENT);
	} else {
		text(10, 9, 0.5f, CLR_DIM, 0, "Type a country...");
	}
	draw_button(254, 4, 62, 26, CLR_KEY, CLR_TEXT, 0.45f, "List (X)");

	// suggestions
	for (int i = 0; i < MAX_SUGGEST; i++) {
		float y = 34 + i * 25;
		bool sel = i == game.suggestSel && i < game.suggestCount;
		C2D_DrawRectSolid(4, y, 0.5f, 312, 23, sel ? RGBA(60, 46, 40, 255) : CLR_PANEL);
		if (sel) C2D_DrawRectSolid(4, y, 0.55f, 3, 23, CLR_ACCENT);
		if (i < game.suggestCount)
			text(12, y + 4, 0.5f, CLR_TEXT, 0, "%s", COUNTRIES[game.suggest[i]].name);
	}
	if (game.suggestCount == 0 && game.input[0])
		text(160, 63, 0.45f, CLR_DIM, C2D_AlignCenter, "No matching country");
	else if (!game.input[0])
		text(160, 63, 0.42f, CLR_DIM, C2D_AlignCenter, "Tap a suggestion or press A to guess it");

	// keys
	for (int i = 0; i < keyCount; i++) {
		Key* k = &keys[i];
		bool down = i == pressedKey && pressedTimer > 0;
		u32 bg = down ? CLR_KEY_DOWN : (k->ch == KEY_ENTER ? RGBA(150, 70, 30, 255) : CLR_KEY);
		char label[8];
		if (k->label) snprintf(label, sizeof(label), "%s", k->label);
		else { label[0] = k->ch; label[1] = 0; }
		draw_button(k->x, k->y, k->w, k->h, bg, CLR_TEXT, k->label ? 0.48f : 0.6f, label);
	}
}

#define LIST_TOP 32
#define LIST_ROW 23
#define LIST_ROWS 9

static void draw_list_view(void)
{
	text(8, 8, 0.5f, CLR_TEXT, 0, game.mode == MODE_BRIDGE ? "Guesses, best first" : "Guesses, closest first");
	draw_button(254, 4, 62, 24, CLR_KEY, CLR_TEXT, 0.45f, "Back (X)");
	int order[MAX_GUESSES];
	int n = sorted_guesses(order);
	if (n == 0) text(160, 110, 0.5f, CLR_DIM, C2D_AlignCenter, "No guesses yet");
	int maxScroll = n > LIST_ROWS ? n - LIST_ROWS : 0;
	if (listScroll > maxScroll) listScroll = maxScroll;
	if (listScroll < 0) listScroll = 0;
	char km[24];
	for (int r = 0; r < LIST_ROWS && r + listScroll < n; r++) {
		int c = order[r + listScroll];
		float y = LIST_TOP + r * LIST_ROW;
		C2D_DrawRectSolid(4, y, 0.5f, 312, LIST_ROW - 2, CLR_PANEL);
		C2D_DrawRectSolid(4, y, 0.55f, 8, LIST_ROW - 2, countryColor[c]);
		text(18, y + 3, 0.45f, CLR_DIM, 0, "%d", r + listScroll + 1);
		text(42, y + 3, 0.48f, CLR_TEXT, 0, "%s", COUNTRIES[c].name);
		if (game.mode == MODE_BRIDGE) {
			RouteStatus rs = bridge_route_status(&game, c);
			text(310, y + 4, 0.42f, countryColor[c], C2D_AlignRight, "%s", route_label(rs));
			continue;
		}
		if (c == game.target) snprintf(km, sizeof(km), "Found!");
		else fmt_km(km, country_distance(c, game.target));
		text(310, y + 3, 0.48f, c == game.target ? CLR_FOUND : CLR_TEXT, C2D_AlignRight, "%s", km);
	}
	if (maxScroll > 0) {
		float h = 205.0f * LIST_ROWS / n;
		float y = LIST_TOP + (205.0f - h) * listScroll / maxScroll;
		C2D_DrawRectSolid(317, y, 0.6f, 2, h, CLR_DIM);
	}
}

static void draw_result_view(void)
{
	if (game.mode == MODE_BRIDGE) {
		text(160, 14, 0.75f, game.won ? CLR_ON : CLR_ACCENT, C2D_AlignCenter, "%s",
		     game.won ? "Bridge built!" : (game.guessCount >= game.maxGuesses ? "Out of guesses" : "Bridge abandoned"));
		char buf[96];
		snprintf(buf, sizeof(buf), "%s to %s", COUNTRIES[game.start].name, COUNTRIES[game.end].name);
		text_fit(160, 42, 0.55f, 300, CLR_TEXT, C2D_AlignCenter, buf);
	} else if (game.won) {
		text(160, 14, 0.75f, CLR_FOUND, C2D_AlignCenter, "Found it!");
		text(160, 42, 0.55f, CLR_TEXT, C2D_AlignCenter, "%s in %d guess%s", COUNTRIES[game.target].name,
		     game.guessCount, game.guessCount == 1 ? "" : "es");
	} else {
		text(160, 14, 0.75f, CLR_ACCENT, C2D_AlignCenter, "Better luck next time");
		text(160, 42, 0.55f, CLR_TEXT, C2D_AlignCenter, "The country was %s", COUNTRIES[game.target].name);
	}
	C2D_DrawRectSolid(20, 72, 0.5f, 280, 76, CLR_PANEL);
	const char* labels[4] = { "Played", "Win %", "Avg guesses", "Streak" };
	char vals[4][16];
	sprintf(vals[0], "%d", stats.played);
	sprintf(vals[1], "%d", stats.played ? stats.wins * 100 / stats.played : 0);
	if (stats.wins) sprintf(vals[2], "%.1f", (float)stats.totalGuesses / stats.wins);
	else sprintf(vals[2], "-");
	sprintf(vals[3], "%d", stats.streak);
	for (int i = 0; i < 4; i++) {
		float x = 20 + 35 + i * 70;
		text(x, 84, 0.75f, CLR_TEXT, C2D_AlignCenter, "%s", vals[i]);
		text(x, 120, 0.4f, CLR_DIM, C2D_AlignCenter, "%s", labels[i]);
	}
	draw_button(20, 162, 135, 40, CLR_ACCENT, RGBA(20, 20, 20, 255), 0.55f, "New game (A)");
	draw_button(165, 162, 135, 40, CLR_KEY, CLR_TEXT, 0.55f, "Guesses (X)");
	text(160, 214, 0.4f, CLR_DIM, C2D_AlignCenter, "Circle Pad: spin   L/R: zoom   START: menu");
}

static void draw_menu_view(void)
{
	text(160, 10, 0.7f, CLR_TEXT, C2D_AlignCenter, "Choose a game");
	for (int i = 0; i < MODE_COUNT; i++) {
		float y = 46 + i * 78;
		bool sel = i == menuSel;
		C2D_DrawRectSolid(16, y, 0.5f, 288, 68, sel ? RGBA(60, 46, 40, 255) : CLR_PANEL);
		C2D_DrawRectSolid(16, y, 0.55f, 4, 68, sel ? CLR_ACCENT : CLR_KEY);
		text(30, y + 6, 0.65f, sel ? CLR_ACCENT : CLR_TEXT, 0, "%s", MODE_NAMES[i]);
		text_fit(30, y + 36, 0.42f, 266, CLR_DIM, 0, MODE_DESC[i]);
	}
	text(160, 214, 0.4f, CLR_DIM, C2D_AlignCenter, "Tap a game or press A    START: quit");
}

static void draw_bottom(void)
{
	if (inMenu) { draw_menu_view(); return; }
	if (bottomView == VIEW_LIST) draw_list_view();
	else if (game.won || game.gaveUp) draw_result_view();
	else draw_keyboard_view();
	if (confirmGiveUp)
		text(160, 226, 0.4f, CLR_ACCENT, C2D_AlignCenter, "Press SELECT again to give up");
}

// ---------------------------------------------------------------- input
static void handle_touch(touchPosition t, u32 kDown, u32 kHeld)
{
	static int dragStartY = -1, dragStartScroll = 0;
	if (bottomView == VIEW_LIST) {
		if (kDown & KEY_TOUCH) {
			if (in_rect(t, 254, 4, 62, 24)) { bottomView = VIEW_KEYBOARD; return; }
			dragStartY = t.py; dragStartScroll = listScroll;
		} else if ((kHeld & KEY_TOUCH) && dragStartY >= 0) {
			listScroll = dragStartScroll - (t.py - dragStartY) / LIST_ROW;
		}
		return;
	}
	if (!(kDown & KEY_TOUCH)) return;
	if (inMenu) {
		for (int i = 0; i < MODE_COUNT; i++)
			if (in_rect(t, 16, 46 + i * 78, 288, 68)) { menuSel = i; new_game((GameMode)i); }
		return;
	}
	if (game.won || game.gaveUp) {
		if (in_rect(t, 20, 162, 135, 40)) new_game(game.mode);
		else if (in_rect(t, 165, 162, 135, 40)) bottomView = VIEW_LIST;
		return;
	}
	if (in_rect(t, 254, 4, 62, 26)) { bottomView = VIEW_LIST; return; }
	for (int i = 0; i < game.suggestCount; i++)
		if (in_rect(t, 4, 34 + i * 25, 312, 23)) { submit_guess(game.suggest[i]); return; }
	for (int i = 0; i < keyCount; i++) {
		Key* k = &keys[i];
		if (!in_rect(t, k->x - 1, k->y - 1, k->w + 2, k->h + 2)) continue;
		pressedKey = i; pressedTimer = 8;
		if (k->ch == KEY_ENTER) submit_guess(game.suggestCount ? game.suggest[game.suggestSel] : -1);
		else type_char(k->ch);
		return;
	}
}

int main(int argc, char* argv[])
{
	gfxInitDefault();
	C3D_Init(C3D_DEFAULT_CMDBUF_SIZE * 4);
	C2D_Init(16384);
	C2D_Prepare();
	C3D_RenderTarget* top = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
	C3D_RenderTarget* bottom = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);
	textBuf = C2D_TextBufNew(6144);

	srand(time(NULL) ^ (unsigned)osGetTime());
	palette_init();
	keyboard_init();
	globe_init();
	for (int i = 0; i < 90; i++) {
		starX[i] = rand() % TOP_W; starY[i] = rand() % SCR_H; starA[i] = 60 + rand() % 180;
	}
	view.lat = 20; view.lon = 10; view.zoom = 1.0f;
	open_menu();

	u64 start = osGetTime();
	while (aptMainLoop()) {
		hidScanInput();
		u32 kDown = hidKeysDown(), kHeld = hidKeysHeld();
		touchPosition touch;
		hidTouchRead(&touch);

		if (inMenu) {
			if (kDown & KEY_START) break;
			if (kDown & KEY_DUP) menuSel = (menuSel + MODE_COUNT - 1) % MODE_COUNT;
			if (kDown & KEY_DDOWN) menuSel = (menuSel + 1) % MODE_COUNT;
			if (kDown & KEY_A) new_game((GameMode)menuSel);
			else handle_touch(touch, kDown, kHeld);
			goto globe_controls;
		}
		if (kDown & KEY_START) { open_menu(); goto globe_controls; }

		bool over = game.won || game.gaveUp;
		if (kDown & KEY_X) bottomView = bottomView == VIEW_LIST ? VIEW_KEYBOARD : VIEW_LIST;
		if (over && (kDown & KEY_A)) new_game(game.mode);
		else if (!over && bottomView == VIEW_KEYBOARD) {
			if ((kDown & KEY_DUP) && game.suggestSel > 0) game.suggestSel--;
			if ((kDown & KEY_DDOWN) && game.suggestSel < game.suggestCount - 1) game.suggestSel++;
			if (kDown & KEY_A) submit_guess(game.suggestCount ? game.suggest[game.suggestSel] : -1);
			if (kDown & KEY_B) type_char(KEY_DEL);
		}
		if (bottomView == VIEW_LIST) {
			if (kDown & KEY_DUP) listScroll--;
			if (kDown & KEY_DDOWN) listScroll++;
			if (kDown & KEY_B) bottomView = VIEW_KEYBOARD;
		}
		if ((kDown & KEY_SELECT) && !over) {
			if (confirmGiveUp) {
				game_give_up(&game);
				finish_game(false);
				refresh_colors();
				if (game.mode == MODE_BRIDGE) focus_between(game.start, game.end);
				else globe_focus(&view, COUNTRIES[game.target].lat, COUNTRIES[game.target].lon);
				confirmGiveUp = false;
			} else confirmGiveUp = true;
		}
		// any other button cancels a pending give-up
		if ((kDown & ~KEY_SELECT) && !(kDown & KEY_SELECT)) confirmGiveUp = false;

		handle_touch(touch, kDown, kHeld);

	globe_controls:;
		circlePosition cp;
		hidCircleRead(&cp);
		float speed = 2.4f / view.zoom;
		bool moved = false;
		if (abs(cp.dx) > 15 || abs(cp.dy) > 15) {
			view.lon += cp.dx / 156.0f * speed;
			view.lat += cp.dy / 156.0f * speed;
			view.animating = false;
			moved = true;
		}
		if (kHeld & KEY_R) { view.zoom *= 1.03f; moved = true; }
		if (kHeld & KEY_L) { view.zoom /= 1.03f; moved = true; }
		if (view.zoom < 1.0f) view.zoom = 1.0f;
		if (view.zoom > 8.0f) view.zoom = 8.0f;
		idleTimer = (moved || kDown) ? 0 : idleTimer + 1;
		bool spin = inMenu || (game.mode == MODE_CLASSIC && game.guessCount == 0);
		if (spin && !view.animating && idleTimer > 120) view.lon += 0.12f;
		globe_view_update(&view);

		if (toastTimer > 0) toastTimer--;
		if (pressedTimer > 0) pressedTimer--;
		float t = (osGetTime() - start) / 1000.0f;

		C2D_TextBufClear(textBuf);
		C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
		C2D_TargetClear(top, CLR_SPACE);
		C2D_SceneBegin(top);
		draw_top(t);
		C2D_TargetClear(bottom, CLR_BG);
		C2D_SceneBegin(bottom);
		draw_bottom();
		C3D_FrameEnd(0);
	}

	C2D_TextBufDelete(textBuf);
	C2D_Fini();
	C3D_Fini();

	gfxExit();
	return 0;
}
