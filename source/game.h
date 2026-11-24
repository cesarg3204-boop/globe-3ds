#pragma once
#include <stdbool.h>

#define MAX_GUESSES 256
#define MAX_INPUT 28
#define MAX_SUGGEST 3

typedef enum { RESULT_NONE, RESULT_NEW, RESULT_REPEAT, RESULT_WIN, RESULT_ENDPOINT, RESULT_LOSE } GuessResult;
typedef enum { MODE_CLASSIC, MODE_BRIDGE, MODE_BRIDGE_HARD, MODE_EXPLORE, MODE_COUNT } GameMode;
static inline bool is_bridge(GameMode m) { return m == MODE_BRIDGE || m == MODE_BRIDGE_HARD; }
// Bridge mode: how a guessed country relates to the shortest land routes between the two ends
typedef enum { ROUTE_ON, ROUTE_NEAR, ROUTE_OFF } RouteStatus;

#define BRIDGE_EXTRA_GUESSES 4
#define MAX_CHAIN 32

typedef struct {
	GameMode mode;
	int target;             // classic: hidden country
	// bridge mode
	int start, end;         // countries to connect
	int minLinks;           // fewest countries needed between start and end
	int maxGuesses;         // 0 = no limit (normal bridge)
	unsigned char hopFromStart[512], hopFromEnd[512]; // land-border steps, 255 = unreachable
	int lastRoute;          // RouteStatus of the last guess
	int chain[MAX_CHAIN];   // winning chain (start..end) or a revealed solution
	int chainLen;

	int guesses[MAX_GUESSES];
	int guessCount;
	int closest;            // index into COUNTRIES of the closest guess so far, -1 if none
	int lastDistance;       // km of the last guess
	int lastTrend;          // -1 colder, 0 first/same, 1 warmer
	bool won, gaveUp;
	char input[MAX_INPUT + 1];
	int suggest[MAX_SUGGEST];
	int suggestCount;
	int suggestSel;
	bool guessed[512];
} Game;

typedef struct {
	int played, wins, totalGuesses, best, streak, bestStreak;
} Stats;

void game_new(Game* g, GameMode mode);
bool bridge_adjacent(int a, int b);
RouteStatus bridge_route_status(const Game* g, int c);
void game_update_suggestions(Game* g);
GuessResult game_guess(Game* g, int country);
void game_give_up(Game* g);

void stats_load(Stats* s, GameMode mode);
void stats_save(const Stats* s, GameMode mode);
