// Numeri fortunati del giorno: estrazione deterministica (uguale per tutto il giorno)
// e palline 3D disegnate per pixel. Indipendente dal display (righe consegnate a "push").
#pragma once
#include <math.h>
#include <stdint.h>

namespace lucky {

typedef uint16_t (*ColFn)(uint8_t r, uint8_t g, uint8_t b);
typedef void (*RowFn)(int x, int y, int w, const uint16_t* px);

static inline uint32_t mix32(uint32_t x) {
  x ^= x >> 16;
  x *= 0x7feb352dU;
  x ^= x >> 15;
  x *= 0x846ca68bU;
  x ^= x >> 16;
  return x;
}

// 6 numeri diversi tra 1 e 90 scelti in base al giorno (giorni dal 1970).
// out[0..4]: i cinque numeri in ordine crescente; out[5]: il numero Jolly.
static inline void numbers(long day, uint8_t out[6]) {
  uint8_t pool[90];
  for (int i = 0; i < 90; i++) pool[i] = (uint8_t)(i + 1);
  uint32_t s = mix32((uint32_t)day * 2654435761U + 0x9E3779B9U);
  for (int i = 0; i < 6; i++) {
    s = mix32(s + 0x6D2B79F5U * (uint32_t)(i + 1));
    int j = i + (int)(s % (uint32_t)(90 - i));
    uint8_t t = pool[i];
    pool[i] = pool[j];
    pool[j] = t;
    out[i] = pool[i];
  }
  for (int i = 1; i < 5; i++) {  // ordina i primi cinque
    uint8_t v = out[i];
    int j = i - 1;
    while (j >= 0 && out[j] > v) {
      out[j + 1] = out[j];
      j--;
    }
    out[j + 1] = v;
  }
}

static inline float clamp01(float v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }
static inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }

// Pallina con luce, riflesso, disco bianco centrale per il numero e ombra. Se glow: alone dorato.
// bgRGB e' il colore dello sfondo (per ombra e bordo anti-aliasing).
static inline void drawBall(int cx, int cy, int R, uint32_t rgb, uint32_t bgRGB, bool glow, ColFn cf, RowFn push,
                            int maxW = 96) {
  const int half = R + (glow ? 8 : 5);  // il margine esterno basta per l'ombra (o per l'alone)
  const float br = (rgb >> 16) & 255, bgc = (rgb >> 8) & 255, bb = rgb & 255;
  const float gr = (bgRGB >> 16) & 255, gg = (bgRGB >> 8) & 255, gb = bgRGB & 255;
  const float Lx = -0.45f, Ly = -0.55f, Lz = 0.70f;
  uint16_t row[96];
  int W = 2 * half;
  if (W > maxW) W = maxW;
  for (int y = cy - half; y < cy + half; y++) {
    if (y < 0 || y >= 240) continue;
    const float dy = y + 0.5f - cy;
    for (int i = 0; i < W; i++) {
      const int x = cx - half + i;
      const float dx = x + 0.5f - cx;
      const float d = sqrtf(dx * dx + dy * dy);
      float r = gr, g = gg, b = gb;
      // ombra morbida sotto la pallina
      {
        const float sx = dx - 3.0f, sy = (dy - 6.0f) * 1.35f;
        const float sd = sqrtf(sx * sx + sy * sy);
        const float a = clamp01((R + 1.0f - sd) / 8.0f) * 0.5f;
        r *= (1 - a);
        g *= (1 - a);
        b *= (1 - a);
      }
      if (glow) {  // anello luminoso
        const float t = clamp01(1.0f - fabsf(d - (R + 4.5f)) / 3.0f) * 0.75f;
        r = lerpf(r, br, t);
        g = lerpf(g, bgc, t);
        b = lerpf(b, bb, t);
      }
      const float cov = clamp01(R + 0.5f - d);
      if (cov > 0) {
        const float nx = dx / R, ny = dy / R;
        const float nz = sqrtf(fmaxf(0.0f, 1.0f - nx * nx - ny * ny));
        float diff = nx * Lx + ny * Ly + nz * Lz;
        if (diff < 0) diff = 0;
        // riflesso speculare: (2(n.L)n - L).z elevato a 32
        float rz = 2.0f * (nx * Lx + ny * Ly + nz * Lz) * nz - Lz;
        if (rz < 0) rz = 0;
        float sp = rz;
        for (int k = 0; k < 5; k++) sp *= sp;
        const float shade = 0.30f + 0.85f * diff;
        float pr = br * shade + 255 * sp * 0.8f;
        float pg = bgc * shade + 255 * sp * 0.8f;
        float pb = bb * shade + 255 * sp * 0.8f;
        // disco bianco con il numero
        const float dr = 0.63f * R;
        const float dd = clamp01(dr - d + 0.5f);
        if (dd > 0) {
          const float wsh = 0.80f + 0.20f * diff;
          pr = lerpf(pr, 250 * wsh, dd);
          pg = lerpf(pg, 250 * wsh, dd);
          pb = lerpf(pb, 246 * wsh, dd);
        }
        r = lerpf(r, pr > 255 ? 255 : pr, cov);
        g = lerpf(g, pg > 255 ? 255 : pg, cov);
        b = lerpf(b, pb > 255 ? 255 : pb, cov);
      }
      row[i] = cf((uint8_t)r, (uint8_t)g, (uint8_t)b);
    }
    push(cx - half, y, W, row);
  }
}

}  // namespace lucky
