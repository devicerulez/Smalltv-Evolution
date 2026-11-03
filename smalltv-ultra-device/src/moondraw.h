// Disco lunare disegnato per pixel (anti-aliasing, mari e crateri, alone, stelle).
// Indipendente dal display: le righe vengono consegnate a una funzione "push".
#pragma once
#include <math.h>
#include <stdint.h>

namespace moon {

typedef uint16_t (*ColFn)(uint8_t r, uint8_t g, uint8_t b);
typedef void (*RowFn)(int x, int y, int w, const uint16_t* px);

static inline uint32_t hash2(uint32_t x, uint32_t y) {
  uint32_t h = x * 374761393u + y * 668265263u;
  h = (h ^ (h >> 13)) * 1274126177u;
  return h ^ (h >> 16);
}
static inline float clamp01(float v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }

// mari (valore > 0 scurisce) e un cratere raggiante (valore < 0 schiarisce); coordinate in raggi lunari
struct Blob { float x, y, r, s; };
static const Blob BLOBS[] = {
    {-0.32f, -0.34f, 0.30f, 0.60f},  // Mare Imbrium
    {0.14f, -0.40f, 0.20f, 0.55f},   // Mare Serenitatis
    {0.36f, -0.08f, 0.24f, 0.50f},   // Mare Tranquillitatis
    {0.52f, 0.20f, 0.14f, 0.45f},    // Mare Fecunditatis
    {-0.58f, 0.04f, 0.30f, 0.50f},   // Oceanus Procellarum
    {-0.22f, 0.22f, 0.14f, 0.40f},   // Mare Nubium
    {0.02f, 0.02f, 0.11f, 0.30f},    // Sinus Medii
    {-0.12f, 0.64f, 0.09f, -0.55f},  // Tycho
    {-0.42f, -0.02f, 0.06f, -0.35f}, // Copernico
};

// Disegna nel riquadro [cx-half, cx+half) x [cy-half, cy+half); righe sopra minY non vengono scritte.
// elong: elongazione Luna-Sole in gradi (0 nuova, 90 primo quarto, 180 piena, 270 ultimo quarto).
static inline void drawDisc(int cx, int cy, int R, int half, float elong, uint32_t bgRGB, uint32_t glowRGB,
                            ColFn cf, RowFn push, int minY = 0) {
  const float ce = cosf(elong * 3.14159265f / 180.0f);
  const bool waxing = elong < 180.0f;
  const float bgR = (bgRGB >> 16) & 255, bgG = (bgRGB >> 8) & 255, bgB = bgRGB & 255;
  const float glR = (glowRGB >> 16) & 255, glG = (glowRGB >> 8) & 255, glB = glowRGB & 255;
  uint16_t row[160];
  const int W = 2 * half > 160 ? 160 : 2 * half;
  for (int y = cy - half; y < cy + half; y++) {
    if (y < minY || y < 0 || y >= 240) continue;
    const float dy = y + 0.5f - cy;
    const float wHalf = sqrtf(fmaxf(0.0f, (float)R * R - dy * dy));
    for (int i = 0; i < W; i++) {
      const int x = cx - half + i;
      const float dx = x + 0.5f - cx;
      const float d = sqrtf(dx * dx + dy * dy);
      // sfondo: alone + stelle
      float r = bgR, g = bgG, b = bgB;
      if (d > R) {
        float t = clamp01(1.0f - (d - R) / (float)(half - R));
        float a = t * t * 0.34f;
        r += (glR - r) * a; g += (glG - g) * a; b += (glB - b) * a;
        uint32_t h = hash2((uint32_t)x, (uint32_t)y);
        if ((h & 0x7F) == 0 && d > R + 6) {
          float s = 0.35f + 0.65f * ((h >> 8) & 255) / 255.0f;
          r += (255 - r) * s; g += (255 - g) * s; b += (255 - b) * s;
        }
      }
      const float cov = clamp01(R + 0.5f - d);
      if (cov > 0) {
        const float u = dx / R, v = dy / R;
        float m = 0;
        for (unsigned k = 0; k < sizeof(BLOBS) / sizeof(BLOBS[0]); k++) {
          float qx = u - BLOBS[k].x, qy = v - BLOBS[k].y;
          float q = (qx * qx + qy * qy) / (BLOBS[k].r * BLOBS[k].r);
          if (q < 1) m += BLOBS[k].s * (1 - q) * (1 - q);
        }
        if (m > 0.85f) m = 0.85f;
        if (m < -0.6f) m = -0.6f;
        const float xt = wHalf * ce;
        float lit = waxing ? (dx - xt) : (-xt - dx);
        lit = clamp01(lit / 3.0f + 0.5f);
        const float rr = d / R;
        const float limb = 1.0f - 0.24f * rr * rr * rr;
        const float shade = (1.0f - 0.38f * m);
        const float lr = 246 * shade * limb, lg = 238 * shade * limb, lb = 210 * shade * limb;
        const float ds = (1.0f - 0.35f * m);
        const float dr = 20 * ds, dg = 27 * ds, db = 52 * ds;  // lato in ombra (luce cenerognola)
        const float pr = dr + (lr - dr) * lit, pg = dg + (lg - dg) * lit, pb = db + (lb - db) * lit;
        r += (pr - r) * cov; g += (pg - g) * cov; b += (pb - b) * cov;
      }
      row[i] = cf((uint8_t)(r > 255 ? 255 : r), (uint8_t)(g > 255 ? 255 : g), (uint8_t)(b > 255 ? 255 : b));
    }
    push(cx - half, y, W, row);
  }
}

}  // namespace moon
