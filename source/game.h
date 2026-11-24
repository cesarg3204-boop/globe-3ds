#pragma once
#include <stdbool.h>

#define MAX_GUESSES 256
#define MAX_INPUT 28
#define MAX_SUGGEST 3

typedef enum { RESULT_NONE, RESULT_NEW, RESULT_REPEAT, RESULT_WIN } GuessResult;

typedef struct {
	int target;
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

void game_new(Game* g);
void game_update_suggestions(Game* g);
GuessResult game_guess(Game* g, int country);
void game_give_up(Game* g);

void stats_load(Stats* s);
void stats_save(const Stats* s);
