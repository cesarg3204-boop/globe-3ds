#include "game.h"
#include "countrydata.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <sys/stat.h>

void game_new(Game* g)
{
	memset(g, 0, sizeof(*g));
	g->closest = -1;
	int pool = 0;
	for (int i = 0; i < COUNTRY_COUNT; i++) if (COUNTRIES[i].target) pool++;
	int pick = rand() % pool;
	for (int i = 0; i < COUNTRY_COUNT; i++) {
		if (!COUNTRIES[i].target) continue;
		if (pick-- == 0) { g->target = i; break; }
	}
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

GuessResult game_guess(Game* g, int c)
{
	if (g->won || g->gaveUp || c < 0) return RESULT_NONE;
	if (g->guessed[c]) return RESULT_REPEAT;
	if (g->guessCount >= MAX_GUESSES) return RESULT_NONE;
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
#define STATS_PATH STATS_DIR "/stats.txt"

void stats_load(Stats* s)
{
	memset(s, 0, sizeof(*s));
	FILE* f = fopen(STATS_PATH, "r");
	if (!f) return;
	if (fscanf(f, "%d %d %d %d %d %d", &s->played, &s->wins, &s->totalGuesses, &s->best,
	           &s->streak, &s->bestStreak) != 6)
		memset(s, 0, sizeof(*s));
	fclose(f);
}

void stats_save(const Stats* s)
{
	mkdir("sdmc:/3ds", 0777);
	mkdir(STATS_DIR, 0777);
	FILE* f = fopen(STATS_PATH, "w");
	if (!f) return;
	fprintf(f, "%d %d %d %d %d %d\n", s->played, s->wins, s->totalGuesses, s->best, s->streak, s->bestStreak);
	fclose(f);
}
