#pragma once
#include <3ds.h>
#include <stdbool.h>

typedef struct {
	float lat, lon;     // centre of view (degrees)
	float zoom;         // 1 = whole globe
	float tlat, tlon;   // animation target
	bool animating;
} GlobeView;

#define GLOBE_RADIUS 100.0f

void globe_init(void);
void globe_view_update(GlobeView* v);
void globe_focus(GlobeView* v, float lat, float lon);
// countryColor: one ABGR colour per country. highlight: country to pulse (-1 none). t: time in seconds.
void globe_draw(const GlobeView* v, const u32* countryColor, float cx, float cy);
// Draw dots for small countries in markerMask (non-zero entries), after the globe.
void globe_draw_markers(const GlobeView* v, const u32* countryColor, const unsigned char* markerMask,
                        int highlight, float t, float cx, float cy);
