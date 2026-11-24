// Orthographic globe renderer using citro2d triangles.
#include "globe.h"
#include "countrydata.h"
#include <citro2d.h>
#include <math.h>
#include <stdlib.h>

#define DEG (3.14159265f / 180.0f)
#define SCREEN_W 400.0f
#define SCREEN_H 240.0f

// per-LOD vertex trig, precomputed once
static float* vCosLat[LOD_COUNT]; static float* vSinLat[LOD_COUNT];
static float* vCosLon[LOD_COUNT]; static float* vSinLon[LOD_COUNT];
// projected vertices of the country being drawn
static float* sx; static float* sy; static float* sz; static float* sl;
static float capCos[512];  // cos of each country's bounding cap radius (with a margin)

void globe_init(void)
{
	int maxCount = 0;
	for (int lod = 0; lod < LOD_COUNT; lod++) {
		int n = VERTEX_COUNT[lod];
		vCosLat[lod] = malloc(sizeof(float) * n * 4);
		vSinLat[lod] = vCosLat[lod] + n; vCosLon[lod] = vSinLat[lod] + n; vSinLon[lod] = vCosLon[lod] + n;
		for (int i = 0; i < n; i++) {
			float lon = VERTS[lod][i * 2] * 0.01f * DEG, lat = VERTS[lod][i * 2 + 1] * 0.01f * DEG;
			vCosLat[lod][i] = cosf(lat); vSinLat[lod][i] = sinf(lat);
			vCosLon[lod][i] = cosf(lon); vSinLon[lod][i] = sinf(lon);
		}
	}
	for (int c = 0; c < COUNTRY_COUNT; c++) {
		for (int lod = 0; lod < LOD_COUNT; lod++)
			if (COUNTRIES[c].vcount[lod] > maxCount) maxCount = COUNTRIES[c].vcount[lod];
		float cap = COUNTRIES[c].cap + 0.5f;
		capCos[c] = cap >= 180.0f ? -1.0f : cosf(cap * DEG);
	}
	sx = malloc(sizeof(float) * maxCount * 4);
	sy = sx + maxCount; sz = sy + maxCount; sl = sz + maxCount;
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
		if (fabsf(dlon) < 0.01f && fabsf(dlat) < 0.01f) v->animating = false;
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

typedef struct { float cl0, sl0, cp0, sp0, R, cx, cy; } Projection;

static Projection make_projection(const GlobeView* v, float cx, float cy)
{
	Projection p = { cosf(v->lon * DEG), sinf(v->lon * DEG), cosf(v->lat * DEG), sinf(v->lat * DEG),
	                 GLOBE_RADIUS * v->zoom, cx, cy };
	return p;
}

// cosine of the angle between the view centre and a point
static float view_cos(const Projection* p, float lat, float lon)
{
	float la = lat * DEG, lo = lon * DEG;
	return p->sp0 * sinf(la) + p->cp0 * cosf(la) * (cosf(lo) * p->cl0 + sinf(lo) * p->sl0);
}

void globe_draw(const GlobeView* v, const u32* countryColor, float cx, float cy)
{
	Projection p = make_projection(v, cx, cy);
	int lod = v->zoom >= 2.5f ? 1 : 0;
	// angular radius of the part of the globe that can be on screen
	float halfDiag = sqrtf(SCREEN_W * SCREEN_W + SCREEN_H * SCREEN_H) / 2.0f / p.R;
	float viewAngle = halfDiag >= 1.0f ? 90.0f : asinf(halfDiag) / DEG;

	for (int c = 0; c < COUNTRY_COUNT; c++) {
		const Country* k = &COUNTRIES[c];
		// skip countries that are entirely outside the visible cap
		float total = viewAngle + k->cap + 0.5f;
		if (total < 180.0f && view_cos(&p, k->lat, k->lon) < cosf(total * DEG)) continue;

		int vb = k->vstart[lod], n = k->vcount[lod];
		const float *cla = vCosLat[lod] + vb, *sla = vSinLat[lod] + vb, *clo = vCosLon[lod] + vb, *slo = vSinLon[lod] + vb;
		for (int i = 0; i < n; i++) {
			float cosdl = clo[i] * p.cl0 + slo[i] * p.sl0;  // cos(lon - lon0)
			float sindl = slo[i] * p.cl0 - clo[i] * p.sl0;  // sin(lon - lon0)
			float e = cla[i] * sindl;
			float nn = p.cp0 * sla[i] - p.sp0 * cla[i] * cosdl;
			float d = p.sp0 * sla[i] + p.cp0 * cla[i] * cosdl;
			sx[i] = cx + p.R * e;
			sy[i] = cy - p.R * nn;
			sz[i] = d;
			float lum = e * LX + nn * LY + d * LZ;
			sl[i] = 0.42f + 0.68f * (lum > 0 ? lum : 0);
		}

		const unsigned short* t = &TRIS[lod][k->tstart[lod] * 3];
		u32 col = countryColor[c];
		for (int i = 0; i < k->tcount[lod]; i++, t += 3) {
			int a = t[0], b = t[1], d = t[2];
			if (sz[a] < -0.05f && sz[b] < -0.05f && sz[d] < -0.05f) continue;
			// off screen?
			if ((sx[a] < 0 && sx[b] < 0 && sx[d] < 0) || (sx[a] > SCREEN_W && sx[b] > SCREEN_W && sx[d] > SCREEN_W)) continue;
			if ((sy[a] < 0 && sy[b] < 0 && sy[d] < 0) || (sy[a] > SCREEN_H && sy[b] > SCREEN_H && sy[d] > SCREEN_H)) continue;
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
	Projection p = make_projection(v, cx, cy);
	for (int c = 0; c < COUNTRY_COUNT; c++) {
		if (!markerMask[c] && c != highlight) continue;
		const Country* k = &COUNTRIES[c];
		float lat = k->lat * DEG, lon = k->lon * DEG;
		float cosdl = cosf(lon) * p.cl0 + sinf(lon) * p.sl0, sindl = sinf(lon) * p.cl0 - cosf(lon) * p.sl0;
		float d = p.sp0 * sinf(lat) + p.cp0 * cosf(lat) * cosdl;
		if (d < 0.05f) continue;
		float x = cx + p.R * cosf(lat) * sindl;
		float y = cy - p.R * (p.cp0 * sinf(lat) - p.sp0 * cosf(lat) * cosdl);
		if (x < -20 || x > SCREEN_W + 20 || y < -20 || y > SCREEN_H + 20) continue;
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

// Which country covers this point? Returns -1 for ocean. Uses the detailed mesh.
int globe_country_at(float lat, float lon)
{
	float px = lon * 100.0f, py = lat * 100.0f;
	for (int c = 0; c < COUNTRY_COUNT; c++) {
		const Country* k = &COUNTRIES[c];
		// cheap reject using the country's bounding cap
		float la = lat * DEG, lo = lon * DEG, kla = k->lat * DEG, klo = k->lon * DEG;
		float cosang = sinf(la) * sinf(kla) + cosf(la) * cosf(kla) * cosf(lo - klo);
		if (cosang < capCos[c]) continue;
		const unsigned short* t = &TRIS[1][k->tstart[1] * 3];
		const short* v = &VERTS[1][k->vstart[1] * 2];
		for (int i = 0; i < k->tcount[1]; i++, t += 3) {
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
