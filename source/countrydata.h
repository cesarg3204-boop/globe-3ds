// Country data generated from Natural Earth (public domain). See tools/gen_countries.py.
#pragma once

typedef struct {
	const char* name;        // display name (ASCII)
	const char* keys;        // '|' separated lowercase letters-only search keys
	float lat, lon;          // label / focus point (degrees)
	float extent;            // size of the largest part, degrees
	unsigned short vstart, vcount;
	unsigned int tstart;
	unsigned short tcount;
	unsigned char shade;     // 1..7, map colouring so neighbours differ
	unsigned char target;    // can be picked as the hidden country
} Country;

extern const int COUNTRY_COUNT;
extern const int VERTEX_COUNT;
extern const int TRIANGLE_COUNT;
extern const Country COUNTRIES[];
extern const short VERTS[];            // lon*100, lat*100 pairs
extern const unsigned short TRIS[];    // indices local to each country's vertex range
extern const unsigned short DIST_KM[]; // COUNTRY_COUNT x COUNTRY_COUNT, border-to-border km

static inline int country_distance(int a, int b) { return DIST_KM[a * COUNTRY_COUNT + b]; }
