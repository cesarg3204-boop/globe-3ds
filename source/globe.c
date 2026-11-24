// Orthographic globe renderer using citro2d triangles.
#include "globe.h"
#include "countrydata.h"
#include <citro2d.h>
#include <math.h>
#include <stdlib.h>

#define DEG (3.14159265f / 180.0f)

static float* vCosLat; static float* vSinLat; static float* vCosLon; static float* vSinLon;
static float* sx; static float* sy; static float* sz; static float* sl;

void globe_init(void)
{
	int n = VERTEX_COUNT;
	vCosLat = malloc(sizeof(float) * n * 8);
	vSinLat = vCosLat + n; vCosLon = vSinLat + n; vSinLon = vCosLon + n;
	sx = vSinLon + n; sy = sx + n; sz = sy + n; sl = sz + n;
	for (int i = 0; i < n; i++) {
		float lon = VERTS[i * 2] * 0.01f * DEG, lat = VERTS[i * 2 + 1] * 0.01f * DEG;
		vCosLat[i] = cosf(lat); vSinLat[i] = sinf(lat);
		vCosLon[i] = cosf(lon); vSinLon[i] = sinf(lon);
	}
}

static float wrap180(float a)
{
	while (a > 180.0f) a -= 360.0f;
	while (a < -180.0f) a += 360.0f;
	return a;
}

void globe_focus(GlobeView* v, float lat, float lon)
{
	v->tlat = lat > 70 ? 70 : (lat < -70 ? -70 : lat);
	v->tlon = lon;
	v->animating = true;
}

void globe_view_update(GlobeView* v)
{
	if (v->animating) {
		float dlon = wrap180(v->tlon - v->lon), dlat = v->tlat - v->lat;
		v->lon += dlon * 0.1f; v->lat += dlat * 0.1f;
		if (fabsf(dlon) < 0.05f && fabsf(dlat) < 0.05f) v->animating = false;
	}
	v->lon = wrap180(v->lon);
	if (v->lat > 89) v->lat = 89;
	if (v->lat < -89) v->lat = -89;
}

static inline u32 shade(u32 c, float f)
{
	if (f > 1.0f) f = 1.0f;
	u32 r = (c & 0xFF) * f, g = ((c >> 8) & 0xFF) * f, b = ((c >> 16) & 0xFF) * f;
	return (c & 0xFF000000) | (b << 16) | (g << 8) | r;
}

// light direction in view space (east, north, towards viewer), normalised
static const float LX = -0.36f, LY = 0.46f, LZ = 0.81f;

static void project(const GlobeView* v, float cx, float cy)
{
	float R = GLOBE_RADIUS * v->zoom;
	float cl0 = cosf(v->lon * DEG), sl0 = sinf(v->lon * DEG);
	float cp0 = cosf(v->lat * DEG), sp0 = sinf(v->lat * DEG);
	for (int i = 0; i < VERTEX_COUNT; i++) {
		float cosdl = vCosLon[i] * cl0 + vSinLon[i] * sl0;  // cos(lon - lon0)
		float sindl = vSinLon[i] * cl0 - vCosLon[i] * sl0;  // sin(lon - lon0)
		float e = vCosLat[i] * sindl;
		float nn = cp0 * vSinLat[i] - sp0 * vCosLat[i] * cosdl;
		float d = sp0 * vSinLat[i] + cp0 * vCosLat[i] * cosdl;
		sx[i] = cx + R * e;
		sy[i] = cy - R * nn;
		sz[i] = d;
		float lum = e * LX + nn * LY + d * LZ;
		sl[i] = 0.42f + 0.68f * (lum > 0 ? lum : 0);
	}
}

void globe_draw(const GlobeView* v, const u32* countryColor, float cx, float cy)
{
	project(v, cx, cy);
	for (int c = 0; c < COUNTRY_COUNT; c++) {
		const Country* k = &COUNTRIES[c];
		const unsigned short* t = &TRIS[k->tstart * 3];
		int vb = k->vstart;
		u32 col = countryColor[c];
		for (int i = 0; i < k->tcount; i++, t += 3) {
			int a = vb + t[0], b = vb + t[1], d = vb + t[2];
			if (sz[a] < -0.05f && sz[b] < -0.05f && sz[d] < -0.05f) continue;
			// CCW (east/north) triangles face the viewer; screen y is flipped so front faces have negative area
			float area = (sx[b] - sx[a]) * (sy[d] - sy[a]) - (sy[b] - sy[a]) * (sx[d] - sx[a]);
			if (area > 0.0f) continue;
			C2D_DrawTriangle(sx[a], sy[a], shade(col, sl[a]), sx[b], sy[b], shade(col, sl[b]),
			                 sx[d], sy[d], shade(col, sl[d]), 0.5f);
		}
	}
}

void globe_draw_markers(const GlobeView* v, const u32* countryColor, const unsigned char* markerMask,
                        int highlight, float t, float cx, float cy)
{
	float R = GLOBE_RADIUS * v->zoom;
	float cl0 = cosf(v->lon * DEG), sl0 = sinf(v->lon * DEG);
	float cp0 = cosf(v->lat * DEG), sp0 = sinf(v->lat * DEG);
	for (int c = 0; c < COUNTRY_COUNT; c++) {
		if (!markerMask[c] && c != highlight) continue;
		const Country* k = &COUNTRIES[c];
		float lat = k->lat * DEG, lon = k->lon * DEG;
		float cosdl = cosf(lon) * cl0 + sinf(lon) * sl0, sindl = sinf(lon) * cl0 - cosf(lon) * sl0;
		float d = sp0 * sinf(lat) + cp0 * cosf(lat) * cosdl;
		if (d < 0.05f) continue;
		float x = cx + R * cosf(lat) * sindl;
		float y = cy - R * (cp0 * sinf(lat) - sp0 * cosf(lat) * cosdl);
		bool small = k->extent * v->zoom < 3.0f;
		if (c == highlight) {
			float pulse = 0.5f + 0.5f * sinf(t * 6.0f);
			float r = 8.0f + 4.0f * pulse;
			u32 ring = C2D_Color32(255, 255, 255, 120 + (int)(120 * pulse));
			for (int s = 0; s < 20; s++) {
				float a0 = s * (6.2831853f / 20), a1 = (s + 1) * (6.2831853f / 20);
				C2D_DrawLine(x + r * cosf(a0), y + r * sinf(a0), ring, x + r * cosf(a1), y + r * sinf(a1), ring, 1.5f, 0.6f);
			}
		}
		if (small && markerMask[c]) {
			C2D_DrawCircleSolid(x, y, 0.7f, 4.0f, C2D_Color32(255, 255, 255, 230));
			C2D_DrawCircleSolid(x, y, 0.7f, 2.8f, countryColor[c]);
		}
	}
}

// Which country covers this point? Returns -1 for ocean. Tests the same triangles that are drawn.
int globe_country_at(float lat, float lon)
{
	float px = lon * 100.0f, py = lat * 100.0f;
	for (int c = 0; c < COUNTRY_COUNT; c++) {
		const Country* k = &COUNTRIES[c];
		const unsigned short* t = &TRIS[k->tstart * 3];
		const short* v = &VERTS[k->vstart * 2];
		for (int i = 0; i < k->tcount; i++, t += 3) {
			float ax = v[t[0] * 2], ay = v[t[0] * 2 + 1], bx = v[t[1] * 2], by = v[t[1] * 2 + 1];
			float cx = v[t[2] * 2], cy = v[t[2] * 2 + 1];
			if ((px < ax && px < bx && px < cx) || (px > ax && px > bx && px > cx)) continue;
			if ((py < ay && py < by && py < cy) || (py > ay && py > by && py > cy)) continue;
			float d1 = (px - bx) * (ay - by) - (ax - bx) * (py - by);
			float d2 = (px - cx) * (by - cy) - (bx - cx) * (py - cy);
			float d3 = (px - ax) * (cy - ay) - (cx - ax) * (py - ay);
			bool neg = d1 < 0 || d2 < 0 || d3 < 0, pos = d1 > 0 || d2 > 0 || d3 > 0;
			if (!(neg && pos)) return c;
		}
	}
	return -1;
}
