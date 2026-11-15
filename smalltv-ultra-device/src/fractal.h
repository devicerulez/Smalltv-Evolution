// Frattali in tempo reale: Mandelbrot e Julia (anche animata) con aritmetica a virgola fissa (Q12, veloce
// sull'ESP8266), colorazione continua (smooth coloring) e palette a coseno. Il disegno e' progressivo:
// prima blocchi grandi, poi sempre piu' fini, a piccole "fette" di tempo cosi' il resto del firmware
// (web, orologio) resta reattivo. Indipendente dal display.
#pragma once
#include <Arduino.h>
#include <math.h>
#include <stdint.h>

namespace fractal {

typedef uint16_t (*ColFn)(uint8_t r, uint8_t g, uint8_t b);
typedef void (*FillFn)(int x, int y, int w, int h, uint16_t c);
typedef void (*RowFn)(int x, int y, int w, const uint16_t* px);

enum Kind : uint8_t { MANDEL, JULIA };

struct Preset {
  const char* name;
  Kind kind;
  float cx, cy, w;  // centro e larghezza della finestra nel piano complesso
  float jr, ji;     // costante di Julia
  uint16_t maxIter;
  float freq;       // quanto in fretta ruotano i colori
  uint8_t pal;      // palette 0..3
  bool morph;       // Julia animata: la costante c percorre un cerchio e i colori scorrono
};

// Finestre scelte per restare nella precisione Q12 (passo di almeno ~6 unita' per pixel).
static const Preset PRESETS[] = {
    {"Julia", JULIA, 0.0f, 0.0f, 3.3f, -0.8f, 0.156f, 90, 38.0f, 0, false},
    {"Julia in movimento", JULIA, 0.0f, 0.0f, 3.3f, 0, 0, 56, 30.0f, 2, true},
    {"Mandelbrot", MANDEL, -0.55f, 0.0f, 3.1f, 0, 0, 80, 34.0f, 1, false},
    {"Julia", JULIA, 0.0f, 0.0f, 3.3f, -0.70176f, -0.3842f, 100, 36.0f, 2, false},
    {"Julia", JULIA, 0.0f, 0.0f, 3.2f, -0.4f, 0.6f, 100, 36.0f, 3, false},
    {"Mandelbrot", MANDEL, -0.16f, 1.03f, 0.5f, 0, 0, 150, 30.0f, 0, false},
    {"Julia in movimento", JULIA, 0.0f, 0.0f, 3.3f, 0, 0, 56, 30.0f, 3, true},
    {"Julia", JULIA, 0.0f, 0.0f, 3.2f, -0.123f, 0.745f, 100, 34.0f, 1, false},
    {"Mandelbrot", MANDEL, 0.30f, 0.50f, 0.5f, 0, 0, 130, 30.0f, 2, false},
    {"Julia", JULIA, 0.0f, 0.0f, 3.3f, 0.285f, 0.01f, 110, 32.0f, 3, false},
};
static const uint8_t PRESET_COUNT = sizeof(PRESETS) / sizeof(PRESETS[0]);

static const uint8_t PASSES[5] = {16, 8, 4, 2, 1};

static inline float flog2(float x) {  // log2 approssimato (basta per i colori)
  union {
    float f;
    uint32_t i;
  } u;
  u.f = x;
  return (float)u.i * 1.1920928955078125e-7f - 126.94269504f;
}

struct Renderer {
  const Preset* pr = &PRESETS[0];
  uint8_t pal[256][3];
  ColFn cf = nullptr;
  int32_t x0 = 0, y0 = 0, step = 0;  // Q16
  int32_t jr = 0, ji = 0;            // Q12
  uint8_t pass = 0;
  int row = 0;  // riga di blocchi in corso
  bool done = true;
  uint32_t iters = 0;  // statistiche
  float ang = 0;       // fase della Julia animata
  int shift = 0;       // scorrimento della palette (animazione)
  uint32_t frame = 0;
  FillFn fill = nullptr;
  RowFn rowFn = nullptr;

  // Palette "a coseno": colore = a + b*cos(2*pi*(c*t + d)), diverse per ogni frattale.
  void buildPalette(uint8_t id, ColFn cf) {
    static const float P[4][12] = {
        {0.55f, 0.45f, 0.50f, 0.45f, 0.45f, 0.45f, 1.0f, 1.0f, 1.0f, 0.00f, 0.15f, 0.30f},  // fuoco/ghiaccio
        {0.50f, 0.50f, 0.50f, 0.50f, 0.50f, 0.50f, 1.0f, 1.0f, 0.5f, 0.80f, 0.90f, 0.30f},  // oro e blu
        {0.50f, 0.40f, 0.60f, 0.50f, 0.50f, 0.40f, 2.0f, 1.0f, 1.0f, 0.50f, 0.20f, 0.25f},  // neon viola/ciano
        {0.50f, 0.50f, 0.50f, 0.50f, 0.50f, 0.50f, 1.0f, 0.7f, 0.4f, 0.00f, 0.15f, 0.20f},  // lava
    };
    const float* p = P[id & 3];
    for (int t = 0; t < 256; t++) {
      const float u = t / 256.0f;
      float c[3];
      for (int k = 0; k < 3; k++) {
        float v = p[k] + p[3 + k] * cosf(6.2831853f * (p[6 + k] * u + p[9 + k]));
        c[k] = v < 0 ? 0 : (v > 1 ? 1 : v);
      }
      pal[t][0] = (uint8_t)(c[0] * 255);
      pal[t][1] = (uint8_t)(c[1] * 255);
      pal[t][2] = (uint8_t)(c[2] * 255);
    }
  }

  void begin(uint8_t presetIndex, ColFn cf, FillFn f, RowFn r, float startAngle = 0.0f) {
    pr = &PRESETS[presetIndex % PRESET_COUNT];
    fill = f;
    rowFn = r;
    this->cf = cf;
    buildPalette(pr->pal, cf);
    x0 = (int32_t)((pr->cx - pr->w * 0.5f) * 65536.0f);
    y0 = (int32_t)((pr->cy - pr->w * 0.5f) * 65536.0f);
    step = (int32_t)(pr->w * 65536.0f / 240.0f);
    jr = (int32_t)(pr->jr * 4096.0f);
    ji = (int32_t)(pr->ji * 4096.0f);
    pass = 0;
    row = 0;
    done = false;
    iters = 0;
    frame = 0;
    shift = 0;
    if (pr->morph) setMorph(startAngle);
  }

  // c = 0.7885 * e^(i*ang): percorso classico delle Julia che si trasformano
  void setMorph(float a) {
    ang = a;
    jr = (int32_t)(0.7885f * cosf(a) * 4096.0f);
    ji = (int32_t)(0.7885f * sinf(a) * 4096.0f);
  }

  // colore del pixel (px,py) dello schermo
  uint16_t color(int px, int py) {
    const int32_t x = (x0 + (int32_t)px * step) >> 4;  // Q12
    const int32_t y = (y0 + (int32_t)py * step) >> 4;
    int32_t zr, zi, cr, ci;
    if (pr->kind == JULIA) {
      zr = x; zi = y; cr = jr; ci = ji;
    } else {
      zr = 0; zi = 0; cr = x; ci = y;
      // punti sicuramente dentro: cardioide e bulbo principale
      const int32_t y2 = (y * y) >> 12;
      const int32_t xm = x - 1024;
      const int32_t q = ((xm * xm) >> 12) + y2;
      if (q < 20000 && ((q * (q + xm)) >> 12) < (y2 >> 2)) return 0;
      const int32_t xb = x + 4096;
      if ((((xb * xb) >> 12) + y2) < 256) return 0;
    }
    const int32_t B = 8 << 12;  // raggio di fuga 8 (|zr| o |zi| oltre B: fuga)
    const int maxIt = pr->maxIter;
    int n = 0;
    for (; n < maxIt; n++) {
      const int32_t zr2 = (zr * zr) >> 12, zi2 = (zi * zi) >> 12;
      const int32_t t = ((zr * zi) >> 11) + ci;
      zr = zr2 - zi2 + cr;
      zi = t;
      if (zr > B || zr < -B || zi > B || zi < -B) break;
    }
    iters += n;
    if (n >= maxIt) return 0;  // interno: nero
    // colorazione continua: nu = n + 1 - log2(log2|z|)
    const float rr = zr * (1.0f / 4096.0f), ii = zi * (1.0f / 4096.0f);
    const float l2 = 0.5f * flog2(rr * rr + ii * ii);  // log2|z| (>= 3)
    const float nu = (float)n + 1.0f - flog2(l2);
    // colori: la radice quadrata concentra le bande vicino all'insieme; lo sfondo lontano si scurisce
    const int idx = (((int)(sqrtf(nu) * pr->freq)) + shift) & 255;
    float k = 0.10f + nu * 0.11f;
    if (k > 1.0f) k = 1.0f;
    return cf((uint8_t)(pal[idx][0] * k), (uint8_t)(pal[idx][1] * k), (uint8_t)(pal[idx][2] * k));
  }

  // Lavora per circa budgetMs millisecondi. Ritorna true quando l'immagine e' completa.
  bool step_(uint32_t budgetMs) {
    if (done) return true;
    const uint32_t t0 = millis();
    uint16_t line[240];
    while (!done) {
      if (pr->morph) {  // frame a blocchi 3x3 (griglia 80x80): colori che scorrono, c che si muove
        for (int bx = 0; bx < 80; bx++) {
          const uint16_t c = color(bx * 3 + 1, row * 3 + 1);
          line[bx * 3] = c;
          line[bx * 3 + 1] = c;
          line[bx * 3 + 2] = c;
        }
        for (int k = 0; k < 3; k++) rowFn(0, row * 3 + k, 240, line);
        if (++row >= 80) {
          row = 0;
          frame++;
          shift += 5;
          setMorph(ang + 0.035f);
        }
        yield();
        if (millis() - t0 >= budgetMs) break;
        continue;
      }
      const int bs = PASSES[pass];
      const int nb = 240 / bs;
      if (bs == 1) {
        for (int x = 0; x < 240; x++) line[x] = color(x, row);
        rowFn(0, row, 240, line);
      } else {
        for (int bx = 0; bx < nb; bx++) fill(bx * bs, row * bs, bs, bs, color(bx * bs + bs / 2, row * bs + bs / 2));
      }
      if (++row >= nb) {
        row = 0;
        if (++pass >= 5) done = true;
      }
      yield();
      if (millis() - t0 >= budgetMs) break;
    }
    return done;
  }
};

}  // namespace fractal
