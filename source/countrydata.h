// Country data generated from Natural Earth (public domain). See tools/gen_countries.py.
#pragma once

// Level of detail: 0 = light mesh for the whole globe, 1 = detailed mesh (small islands too) for close zoom
#define LOD_COUNT 2

typedef struct {
	const char* name;        // display name (ASCII)
	const char* keys;        // '|' separated lowercase letters-only search keys
	float lat, lon;          // label / focus point (degrees)
	float extent;            // size of the largest part, degrees
	float cap;               // every vertex lies within this many degrees of (lat, lon)
	unsigned int vstart[LOD_COUNT];
	unsigned short vcount[LOD_COUNT];
	unsigned int tstart[LOD_COUNT];
	unsigned short tcount[LOD_COUNT];
	unsigned char shade;     // 1..7, map colouring so neighbours differ
	unsigned char target;    // can be picked as the hidden country
} Country;

extern const int COUNTRY_COUNT;
extern const int VERTEX_COUNT[LOD_COUNT];
extern const int TRIANGLE_COUNT[LOD_COUNT];
extern const Country COUNTRIES[];
extern const short* const VERTS[LOD_COUNT];          // lon*100, lat*100 pairs
extern const unsigned short* const TRIS[LOD_COUNT];  // indices local to each country's vertex range
extern const unsigned short DIST_KM[];               // COUNTRY_COUNT x COUNTRY_COUNT, border-to-border km

static inline int country_distance(int a, int b) { return DIST_KM[a * COUNTRY_COUNT + b]; }
