#include "game.h"
#include "countrydata.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <sys/stat.h>

static int random_target(void)
{
	int pool = 0;
	for (int i = 0; i < COUNTRY_COUNT; i++) if (COUNTRIES[i].target) pool++;
	int pick = rand() % pool;
	for (int i = 0; i < COUNTRY_COUNT; i++) {
		if (!COUNTRIES[i].target) continue;
		if (pick-- == 0) return i;
	}
	return 0;
}

static int find_country(const char* name)
{
	for (int i = 0; i < COUNTRY_COUNT; i++)
		if (strcmp(COUNTRIES[i].name, name) == 0) return i;
	return -1;
}

// Borders that only exist through overseas territories; they make silly bridges across oceans.
static const char* OVERSEAS[][2] = {
	{ "France", "Brazil" }, { "France", "Suriname" }, { "Netherlands", "Saint-Martin" },
};

bool bridge_adjacent(int a, int b)
{
	if (a == b || !COUNTRIES[a].target || !COUNTRIES[b].target) return false;
	if (country_distance(a, b) != 0) return false;
	static int overseas[sizeof(OVERSEAS) / sizeof(OVERSEAS[0])][2];
	static bool init = false;
	int n = sizeof(OVERSEAS) / sizeof(OVERSEAS[0]);
	if (!init) {
		for (int i = 0; i < n; i++) { overseas[i][0] = find_country(OVERSEAS[i][0]); overseas[i][1] = find_country(OVERSEAS[i][1]); }
		init = true;
	}
	for (int i = 0; i < n; i++)
		if ((overseas[i][0] == a && overseas[i][1] == b) || (overseas[i][0] == b && overseas[i][1] == a)) return false;
	return true;
}

// Breadth-first search over land borders. allowed == NULL means every country may be used.
// Returns hop counts in hop[] and the parent of each visited country in parent[] (may be NULL).
static void bfs(int from, const bool* allowed, unsigned char* hop, short* parent)
{
	static short queue[512];
	int head = 0, tail = 0;
	memset(hop, 255, COUNTRY_COUNT);
	hop[from] = 0;
	if (parent) parent[from] = -1;
	queue[tail++] = from;
	while (head < tail) {
		int x = queue[head++];
		if (hop[x] >= 254) continue;
		for (int y = 0; y < COUNTRY_COUNT; y++) {
			if (hop[y] != 255 || (allowed && !allowed[y]) || !bridge_adjacent(x, y)) continue;
			hop[y] = hop[x] + 1;
			if (parent) parent[y] = x;
			queue[tail++] = y;
		}
	}
}

static int chain_from_parents(const short* parent, int start, int end, int* out)
{
	int tmp[MAX_CHAIN], n = 0;
	for (int x = end; x >= 0 && n < MAX_CHAIN; x = parent[x]) { tmp[n++] = x; if (x == start) break; }
	for (int i = 0; i < n; i++) out[i] = tmp[n - 1 - i];
	return n;
}

void game_new(Game* g, GameMode mode)
{
	memset(g, 0, sizeof(*g));
	g->mode = mode;
	g->closest = -1;
	if (mode == MODE_CLASSIC) {
		g->target = random_target();
		return;
	}
	// Bridge: pick two countries 3..7 border crossings apart (2..6 countries in between)
	for (;;) {
		g->start = random_target();
		bfs(g->start, NULL, g->hopFromStart, NULL);
		int candidates[512], n = 0;
		for (int i = 0; i < COUNTRY_COUNT; i++)
			if (g->hopFromStart[i] >= 3 && g->hopFromStart[i] <= 7) candidates[n++] = i;
		if (n == 0) continue;
		g->end = candidates[rand() % n];
		break;
	}
	short parent[512];
	bfs(g->end, NULL, g->hopFromEnd, parent);
	g->minLinks = g->hopFromStart[g->end] - 1;
	g->maxGuesses = mode == MODE_BRIDGE_HARD ? g->minLinks + BRIDGE_EXTRA_GUESSES : 0;
	g->target = g->end;
	g->guessed[g->start] = g->guessed[g->end] = true;
	// keep one shortest route to reveal if the player runs out of guesses.
	// parent[] points towards the end, so walking it from the start already reads start..end.
	int n = 0;
	for (int x = g->start; x >= 0 && n < MAX_CHAIN; x = parent[x]) g->chain[n++] = x;
	g->chainLen = n;
}

RouteStatus bridge_route_status(const Game* g, int c)
{
	int s = g->hopFromStart[c], e = g->hopFromEnd[c], best = g->hopFromStart[g->end];
	if (s == 255 || e == 255) return ROUTE_OFF;
	if (s + e == best) return ROUTE_ON;
	if (s + e <= best + 2) return ROUTE_NEAR;
	return ROUTE_OFF;
}

// 0 = whole-name prefix, 1 = alias prefix, 2 = word inside a key, -1 = no match
static int match_score(const char* keys, const char* q, int qlen)
{
	int best = -1, first = 1;
	const char* k = keys;
	while (*k) {
		const char* end = strchr(k, '|');
		int len = end ? (int)(end - k) : (int)strlen(k);
		if (len >= qlen) {
			if (strncmp(k, q, qlen) == 0) {
				int s = first ? 0 : 1;
				if (best < 0 || s < best) best = s;
			} else if (best < 0 || best > 2) {
				for (int i = 1; i + qlen <= len; i++)
					if (strncmp(k + i, q, qlen) == 0) { best = 2; break; }
			}
		}
		first = 0;
		if (!end) break;
		k = end + 1;
	}
	return best;
}

void game_update_suggestions(Game* g)
{
	char q[MAX_INPUT + 1];
	int qlen = 0;
	for (const char* p = g->input; *p; p++)
		if (isalpha((unsigned char)*p)) q[qlen++] = tolower((unsigned char)*p);
	q[qlen] = 0;
	g->suggestCount = 0;
	g->suggestSel = 0;
	if (qlen == 0) return;

	int scores[MAX_SUGGEST];
	for (int i = 0; i < COUNTRY_COUNT; i++) {
		if (g->guessed[i]) continue;
		if (is_bridge(g->mode) && !COUNTRIES[i].target) continue;
		int s = match_score(COUNTRIES[i].keys, q, qlen);
		if (s < 0) continue;
		// insertion into a tiny sorted list (score, then alphabetical = index order)
		int pos = g->suggestCount;
		while (pos > 0 && scores[pos - 1] > s) pos--;
		if (pos >= MAX_SUGGEST) continue;
		int n = g->suggestCount < MAX_SUGGEST ? g->suggestCount : MAX_SUGGEST - 1;
		for (int j = n; j > pos; j--) { scores[j] = scores[j - 1]; g->suggest[j] = g->suggest[j - 1]; }
		scores[pos] = s;
		g->suggest[pos] = i;
		if (g->suggestCount < MAX_SUGGEST) g->suggestCount++;
	}
}

static GuessResult bridge_guess(Game* g, int c)
{
	if (c == g->start || c == g->end) return RESULT_ENDPOINT;
	if (g->guessed[c]) return RESULT_REPEAT;
	g->guessed[c] = true;
	g->guesses[g->guessCount++] = c;
	g->lastRoute = bridge_route_status(g, c);
	g->input[0] = 0;
	game_update_suggestions(g);

	// is there now a chain start -> guessed countries -> end?
	bool allowed[512] = { false };
	for (int i = 0; i < g->guessCount; i++) allowed[g->guesses[i]] = true;
	allowed[g->start] = allowed[g->end] = true;
	unsigned char hop[512];
	short parent[512];
	bfs(g->start, allowed, hop, parent);
	if (hop[g->end] != 255) {
		g->chainLen = chain_from_parents(parent, g->start, g->end, g->chain);
		g->won = true;
		return RESULT_WIN;
	}
	if (g->maxGuesses && g->guessCount >= g->maxGuesses) {
		g->gaveUp = true;
		return RESULT_LOSE;
	}
	return RESULT_NEW;
}

GuessResult game_guess(Game* g, int c)
{
	if (g->won || g->gaveUp || c < 0) return RESULT_NONE;
	if (g->guessCount >= MAX_GUESSES) return RESULT_NONE;
	if (is_bridge(g->mode)) return bridge_guess(g, c);
	if (g->guessed[c]) return RESULT_REPEAT;
	g->guessed[c] = true;
	g->guesses[g->guessCount++] = c;
	int d = country_distance(c, g->target);
	int prevBest = g->closest >= 0 ? country_distance(g->closest, g->target) : -1;
	g->lastDistance = d;
	if (prevBest < 0) g->lastTrend = 0;
	else g->lastTrend = d < prevBest ? 1 : (d > prevBest ? -1 : 0);
	if (prevBest < 0 || d < prevBest || c == g->target) g->closest = c;
	g->input[0] = 0;
	game_update_suggestions(g);
	if (c == g->target) { g->won = true; return RESULT_WIN; }
	return RESULT_NEW;
}

void game_give_up(Game* g)
{
	if (g->won) return;
	g->gaveUp = true;
}

#define STATS_DIR "sdmc:/3ds/globe3ds"
static const char* STATS_FILES[MODE_COUNT] = { STATS_DIR "/stats.txt", STATS_DIR "/stats_bridge.txt",
                                                  STATS_DIR "/stats_bridge_hard.txt" };

void stats_load(Stats* s, GameMode mode)
{
	memset(s, 0, sizeof(*s));
	FILE* f = fopen(STATS_FILES[mode], "r");
	if (!f) return;
	if (fscanf(f, "%d %d %d %d %d %d", &s->played, &s->wins, &s->totalGuesses, &s->best,
	           &s->streak, &s->bestStreak) != 6)
		memset(s, 0, sizeof(*s));
	fclose(f);
}

void stats_save(const Stats* s, GameMode mode)
{
	mkdir("sdmc:/3ds", 0777);
	mkdir(STATS_DIR, 0777);
	FILE* f = fopen(STATS_FILES[mode], "w");
	if (!f) return;
	fprintf(f, "%d %d %d %d %d %d\n", s->played, s->wins, s->totalGuesses, s->best, s->streak, s->bestStreak);
	fclose(f);
}
