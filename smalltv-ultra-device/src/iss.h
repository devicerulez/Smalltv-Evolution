// Radar ISS: modello orbitale semplificato, mappa del mondo a punti, geometria e ricerca dei passaggi.
// La posizione reale arriva da Open Notify (iss-now.json); i passaggi sopra la citta' si calcolano qui,
// perche' l'endpoint iss-pass.json di Open Notify non e' piu' disponibile.
// Mappa: Natural Earth (dominio pubblico), 74x31 celle di ~4.86 gradi, latitudini da +75 a -75.
#pragma once
#include <math.h>
#include <stdint.h>

namespace iss {

static const int MAP_COLS = 74, MAP_ROWS = 31;
static const float MAP_LAT_TOP = 75.0f, MAP_LAT_SPAN = 150.0f;

static const uint8_t MAP_BITS[MAP_ROWS * 10] PROGMEM = {
  0x00, 0x1F, 0xFC, 0x7F, 0x80, 0x00, 0x9F, 0xFF, 0xF0, 0x00,
  0xDF, 0xFF, 0xFF, 0x3E, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xC0,
  0x1F, 0xFF, 0xFF, 0x38, 0x43, 0xFF, 0xFF, 0xFF, 0xFF, 0xC0,
  0x0C, 0xFF, 0xC7, 0x00, 0x0B, 0xFF, 0xFF, 0xFF, 0xCE, 0x00,
  0x00, 0x3F, 0xFF, 0xC0, 0x1F, 0xFF, 0xFF, 0xFF, 0xE4, 0x00,
  0x00, 0x1F, 0xFF, 0xC0, 0x0F, 0xFF, 0xFF, 0xFF, 0xE0, 0x00,
  0x00, 0x1F, 0xFF, 0x00, 0x1F, 0xEE, 0xFF, 0xFF, 0xA0, 0x00,
  0x00, 0x1F, 0xFC, 0x00, 0x1A, 0xFE, 0xFF, 0xFF, 0x40, 0x00,
  0x00, 0x0F, 0xF8, 0x00, 0x1F, 0x0F, 0xFF, 0xFD, 0xC0, 0x00,
  0x00, 0x07, 0xF8, 0x00, 0x3F, 0xFF, 0xFF, 0xFC, 0x00, 0x00,
  0x00, 0x03, 0x88, 0x00, 0x3F, 0xFF, 0xFF, 0xFC, 0x00, 0x00,
  0x00, 0x01, 0xE6, 0x00, 0x7F, 0xFF, 0x8E, 0xF0, 0x00, 0x00,
  0x00, 0x00, 0x30, 0x00, 0x7F, 0xFF, 0x0C, 0x74, 0x00, 0x00,
  0x00, 0x00, 0x1F, 0x80, 0x3F, 0xFF, 0x0C, 0x62, 0x00, 0x00,
  0x00, 0x00, 0x07, 0xE0, 0x1B, 0xFE, 0x00, 0x48, 0x00, 0x00,
  0x00, 0x00, 0x0F, 0xF0, 0x01, 0xFC, 0x00, 0x7C, 0x00, 0x00,
  0x00, 0x00, 0x0F, 0xFC, 0x01, 0xF8, 0x00, 0x2C, 0xE8, 0x00,
  0x00, 0x00, 0x07, 0xFC, 0x01, 0xF8, 0x00, 0x00, 0x30, 0x00,
  0x00, 0x00, 0x07, 0xF8, 0x01, 0xFF, 0x00, 0x03, 0xA0, 0x00,
  0x00, 0x00, 0x03, 0xF8, 0x01, 0xFA, 0x00, 0x07, 0xF0, 0x00,
  0x00, 0x00, 0x03, 0xF0, 0x00, 0xF2, 0x00, 0x0F, 0xF0, 0x00,
  0x00, 0x00, 0x03, 0xE0, 0x00, 0xF0, 0x00, 0x0F, 0xF8, 0x00,
  0x00, 0x00, 0x03, 0xC0, 0x00, 0x60, 0x00, 0x06, 0xF0, 0x00,
  0x00, 0x00, 0x03, 0x80, 0x00, 0x00, 0x00, 0x00, 0x30, 0x40,
  0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x80,
  0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x01, 0x00, 0x00, 0x07, 0xE7, 0xFF, 0xF8, 0x00,
  0x00, 0x08, 0xFF, 0x80, 0x3F, 0xFF, 0xFF, 0xFF, 0xFF, 0x00,
};

inline bool landAt(int c, int r) {
  if (c < 0 || c >= MAP_COLS || r < 0 || r >= MAP_ROWS) return false;
  return (pgm_read_byte(&MAP_BITS[r * 10 + (c >> 3)]) >> (7 - (c & 7))) & 1;
}

static const float PI_F = 3.14159265f, TWO_PI_F = 6.2831853f, DEG = 0.017453293f;
static const float INC = 51.64f * 0.017453293f;
static const float SIN_I = 0.78374f, COS_I = 0.62109f;   // seno e coseno dell'inclinazione (51.64 gradi)
static const float RE = 6371.0f, ALT_KM = 420.0f;
static const float OMEGA_E = 7.2921159e-5f;               // rotazione terrestre (rad/s)
static const float NODE_DOT = -5.0f * 0.017453293f / 86400.0f;  // regressione del nodo (J2), rad/s
static const float N_DEF = 15.512f * 6.2831853f / 86400.0f;     // velocita' angolare lungo l'orbita (rad/s)
static const float MIN_EL = 10.0f;                         // elevazione minima di un passaggio (gradi)

inline float wrapPi(float a) {
  while (a > PI_F) a -= TWO_PI_F;
  while (a < -PI_F) a += TWO_PI_F;
  return a;
}
// latitudine geodetica <-> geocentrica (differenza fino a 0.19 gradi)
inline float toGeoc(float latDeg) { return latDeg - 0.1924f * sinf(2.0f * latDeg * DEG); }
inline float toGeod(float latDeg) { return latDeg + 0.1924f * sinf(2.0f * latDeg * DEG); }

struct Model {
  bool ok;
  uint32_t t0;       // istante dell'ancora (secondi unix)
  float u0;          // argomento di latitudine all'istante t0 (rad)
  float lonNode0;    // longitudine geografica del nodo ascendente a t0 (rad)
  float n;           // velocita' angolare (rad/s)
};

// costruisce il modello da una posizione misurata (lat/lon in gradi, asc = sta salendo di latitudine)
inline bool fit(uint32_t t, float latDeg, float lonDeg, bool asc, float n, Model& m) {
  float s = sinf(toGeoc(latDeg) * DEG) / SIN_I;
  if (s > 1.0f) s = 1.0f;
  if (s < -1.0f) s = -1.0f;
  float u = asinf(s);
  if (!asc) u = PI_F - u;
  const float dl = atan2f(COS_I * sinf(u), cosf(u));
  m.u0 = u;
  m.lonNode0 = wrapPi(lonDeg * DEG - dl);
  m.t0 = t;
  m.n = n;
  m.ok = true;
  return true;
}

// argomento di latitudine all'istante t, ridotto a 0..2pi
inline float argLat(const Model& m, uint32_t t) {
  const float dt = (float)(int32_t)(t - m.t0);
  float u = fmodf(m.u0 + m.n * dt, TWO_PI_F);
  if (u < 0) u += TWO_PI_F;
  return u;
}

// posizione sulla Terra (gradi, latitudine geodetica)
inline void pos(const Model& m, uint32_t t, float& latDeg, float& lonDeg) {
  const float dt = (float)(int32_t)(t - m.t0);
  const float u = argLat(m, t);
  const float su = sinf(u);
  float lat = asinf(SIN_I * su) / DEG;
  const float lon = m.lonNode0 + (NODE_DOT - OMEGA_E) * dt + atan2f(COS_I * su, cosf(u));
  latDeg = toGeod(lat);
  lonDeg = wrapPi(lon) / DEG;
}

// sta salendo di latitudine?
inline bool ascending(const Model& m, uint32_t t) { return cosf(argLat(m, t)) >= 0.0f; }

// stima migliore della velocita' angolare tra due misure (ancora e nuova) lontane nel tempo
inline bool refineN(float uA, uint32_t tA, float uB, uint32_t tB, float nGuess, float& nOut) {
  const float dt = (float)(int32_t)(tB - tA);
  if (dt < 1500.0f) return false;
  float du = uB - uA;
  const float k = floorf((nGuess * dt - du) / TWO_PI_F + 0.5f);
  const float est = (du + k * TWO_PI_F) / dt;
  if (fabsf(est / N_DEF - 1.0f) > 0.01f) return false;
  nOut = est;
  return true;
}

// distanza angolare (rad) e elevazione (gradi) dell'ISS vista da un osservatore
inline float cosPsi(float latO, float lonO, float latS, float lonS) {
  const float po = toGeoc(latO) * DEG, ps = toGeoc(latS) * DEG;
  return sinf(po) * sinf(ps) + cosf(po) * cosf(ps) * cosf((lonS - lonO) * DEG);
}
inline float elevFromCos(float cp) {
  if (cp > 1.0f) cp = 1.0f;
  const float sp = sqrtf(1.0f - cp * cp);
  return atan2f(cp - RE / (RE + ALT_KM), sp) / DEG;
}

// punto subsolare (gradi) all'istante t
inline void sunPoint(uint32_t t, float& latDeg, float& lonDeg) {
  const double n = (double)t / 86400.0 - 10957.5;  // giorni da J2000
  const double L = 280.460 + 0.9856474 * n;
  const double g = (357.528 + 0.9856003 * n) * 0.017453292519943295;
  const double lam = (L + 1.915 * sin(g) + 0.020 * sin(2.0 * g)) * 0.017453292519943295;
  const double eps = (23.439 - 0.0000004 * n) * 0.017453292519943295;
  const double dec = asin(sin(eps) * sin(lam));
  const double ra = atan2(cos(eps) * sin(lam), cos(lam));
  double gmst = fmod(280.46061837 + 360.98564736629 * n, 360.0) * 0.017453292519943295;
  double lon = ra - gmst;
  while (lon > 3.14159265358979) lon -= 6.28318530717959;
  while (lon < -3.14159265358979) lon += 6.28318530717959;
  latDeg = (float)(dec / 0.017453292519943295);
  lonDeg = (float)(lon / 0.017453292519943295);
}

// seno dell'elevazione del Sole per un osservatore
inline float sunSinEl(float latO, float lonO, float sLat, float sLon) {
  const float po = latO * DEG, sd = sLat * DEG;
  return sinf(po) * sinf(sd) + cosf(po) * cosf(sd) * cosf((lonO - sLon) * DEG);
}

// l'ISS (lat/lon in gradi) e' illuminata dal Sole?
inline bool issSunlit(float lat, float lon, float sLat, float sLon) {
  const float r = RE + ALT_KM;
  const float cl = cosf(lat * DEG), sl = sinf(lat * DEG);
  const float x = r * cl * cosf(lon * DEG), y = r * cl * sinf(lon * DEG), z = r * sl;
  const float cd = cosf(sLat * DEG);
  const float sx = cd * cosf(sLon * DEG), sy = cd * sinf(sLon * DEG), sz = sinf(sLat * DEG);
  const float dot = x * sx + y * sy + z * sz;
  if (dot >= 0) return true;
  const float px = x - dot * sx, py = y - dot * sy, pz = z - dot * sz;
  return (px * px + py * py + pz * pz) > RE * RE;
}

struct Pass {
  bool valid;
  uint32_t rise;     // inizio (elevazione >= MIN_EL)
  uint16_t dur;      // durata in secondi
  uint8_t maxEl;     // elevazione massima (gradi)
  bool vis;          // visibile a occhio nudo (ISS illuminata e cielo buio)
};

// ricerca incrementale dei passaggi: si chiama a piccoli passi dal loop() per non bloccare il resto
struct Scan {
  enum St : uint8_t { IDLE, COARSE, FINE, DONE } st;
  Model m;
  float latO, lonO;
  uint32_t t;           // istante in esame
  uint32_t tBegin;      // inizio della ricerca
  uint32_t tEnd;        // limite della ricerca
  uint32_t fineStart;
  bool inPass;
  uint32_t rise, last;
  float maxEl;
  bool vis;
  Pass first, firstVis;
  bool refreshed;       // risultati completi
};

inline void scanStart(Scan& s, const Model& m, float latO, float lonO, uint32_t now, uint32_t horizonSec) {
  s.st = Scan::COARSE;
  s.m = m;
  s.latO = latO;
  s.lonO = lonO;
  s.t = now;
  s.tBegin = now;
  s.tEnd = now + horizonSec;
  s.inPass = false;
  s.first.valid = false;
  s.firstVis.valid = false;
  s.refreshed = false;
}

inline float elevAt(const Scan& s, uint32_t t, float* latS = nullptr, float* lonS = nullptr) {
  float la, lo;
  pos(s.m, t, la, lo);
  if (latS) *latS = la;
  if (lonS) *lonS = lo;
  return elevFromCos(cosPsi(s.latO, s.lonO, la, lo));
}

inline void scanFinishPass(Scan& s) {
  Pass p;
  p.valid = true;
  p.rise = s.rise;
  p.dur = (uint16_t)(s.last - s.rise + 10);
  p.maxEl = (uint8_t)(s.maxEl + 0.5f);
  p.vis = s.vis;
  if (!s.first.valid) s.first = p;
  if (p.vis && !s.firstVis.valid) s.firstVis = p;
  s.inPass = false;
}

// esegue fino a maxSteps campioni; restituisce true quando la ricerca e' finita
inline bool scanStep(Scan& s, int maxSteps) {
  for (int i = 0; i < maxSteps; i++) {
    if (s.st == Scan::IDLE || s.st == Scan::DONE) return true;
    if (s.t >= s.tEnd || (s.first.valid && s.firstVis.valid)) {
      if (s.inPass) scanFinishPass(s);
      s.st = Scan::DONE;
      s.refreshed = true;
      return true;
    }
    float la, lo;
    const float el = elevAt(s, s.t, &la, &lo);
    if (s.st == Scan::COARSE) {
      if (el >= MIN_EL - 4.0f) {
        s.st = Scan::FINE;
        s.t = (s.t >= s.tBegin + 60) ? s.t - 60 : s.tBegin;
        s.fineStart = s.t;
        s.inPass = false;
        s.maxEl = 0;
        s.vis = false;
      } else {
        s.t += 60;
      }
    } else {  // FINE: passo di 10 s
      if (el >= MIN_EL) {
        if (!s.inPass) {
          s.inPass = true;
          s.rise = s.t;
          s.maxEl = 0;
          s.vis = false;
        }
        if (el > s.maxEl) s.maxEl = el;
        s.last = s.t;
        float sl, so;
        sunPoint(s.t, sl, so);
        if (!s.vis && sunSinEl(s.latO, s.lonO, sl, so) < -0.105f && issSunlit(la, lo, sl, so)) s.vis = true;  // Sole sotto -6 gradi
        s.t += 10;
      } else if (s.inPass) {
        scanFinishPass(s);
        s.st = Scan::COARSE;
        s.t += 60;
      } else if (s.t > s.fineStart + 240) {
        s.st = Scan::COARSE;
        s.t += 60;
      } else {
        s.t += 10;
      }
    }
  }
  return false;
}

}  // namespace iss
