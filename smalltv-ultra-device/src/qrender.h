// Disegno di testo UTF-8 con font bitmap anti-aliased (qfonts.h): accenti, a capo automatico.
// Non dipende dal display: le righe di pixel vengono consegnate a una funzione "push".
#pragma once
#include <Arduino.h>
#include <pgmspace.h>
#include "qfonts.h"

namespace qr {

typedef void (*PushFn)(int x, int y, int w, const uint16_t* px);  // una riga di pixel

static const int MAXG = 48;  // glifi massimi per riga

// UTF-8 -> codepoint (ritorna i byte consumati)
static inline int utf8(const char* s, uint16_t& cp) {
  uint8_t c = (uint8_t)s[0];
  if (c < 0x80) { cp = c; return 1; }
  if ((c & 0xE0) == 0xC0 && s[1]) { cp = ((c & 0x1F) << 6) | (s[1] & 0x3F); return 2; }
  if ((c & 0xF0) == 0xE0 && s[1] && s[2]) { cp = ((c & 0x0F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F); return 3; }
  cp = '?';
  return 1;
}

static inline bool glyph(const QFont& f, uint16_t cp, QGlyph& g) {
  int lo = 0, hi = (int)f.n - 1;
  while (lo <= hi) {
    int mid = (lo + hi) >> 1;
    uint16_t v = pgm_read_word(&f.cps[mid]);
    if (v == cp) { memcpy_P(&g, &f.g[mid], sizeof(QGlyph)); return true; }
    if (v < cp) lo = mid + 1; else hi = mid - 1;
  }
  return false;
}

// larghezza in 1/8 di pixel
static inline int width8(const QFont& f, const char* s, int len, int track8 = 0) {
  int w = 0;
  for (int i = 0; i < len;) {
    uint16_t cp;
    i += utf8(s + i, cp);
    QGlyph g;
    if (!glyph(f, cp, g) && !glyph(f, '?', g)) continue;
    w += g.adv8 + track8;
  }
  return w;
}

struct Line { int16_t start, len; int16_t w8; };

// A capo automatico sugli spazi. Ritorna il numero di righe, oppure -1 se superano maxLines.
static inline int layout(const QFont& f, const char* s, int maxW, Line* out, int maxLines) {
  const int maxW8 = maxW * 8;
  QGlyph sp;
  int space8 = glyph(f, ' ', sp) ? sp.adv8 : 24;
  int n = 0, len = (int)strlen(s);
  int ls = 0, lw = 0, pos = 0;
  bool empty = true;
  while (pos <= len) {
    int e = pos;
    while (e < len && s[e] != ' ') e++;
    int ww = width8(f, s + pos, e - pos);
    if (!empty && lw + space8 + ww > maxW8) {
      if (n >= maxLines) return -1;
      out[n++] = {(int16_t)ls, (int16_t)(pos - 1 - ls), (int16_t)lw};
      ls = pos; lw = ww;
    } else {
      lw = empty ? ww : lw + space8 + ww;
    }
    empty = false;
    pos = e + 1;
  }
  if (n >= maxLines) return -1;
  out[n++] = {(int16_t)ls, (int16_t)(len - ls), (int16_t)lw};
  return n;
}

// Disegna una riga: x = bordo sinistro, y = bordo alto; lut = 16 colori da sfondo (0) a testo (15)
static inline void drawLine(const QFont& f, const char* s, int len, int x, int y, int W, const uint16_t* lut,
                            PushFn push, int track8 = 0, int indent = 0) {
  struct G { const uint8_t* bmp; int16_t x; uint8_t w, h; int8_t yo; } gs[MAXG];
  int ng = 0, pen8 = 0;
  for (int i = 0; i < len && ng < MAXG;) {
    uint16_t cp;
    i += utf8(s + i, cp);
    QGlyph g;
    if (!glyph(f, cp, g) && !glyph(f, '?', g)) continue;
    if (g.w) gs[ng++] = {f.bmp + g.off, (int16_t)(((pen8 + 4) >> 3) + g.xo), g.w, g.h, g.yo};
    pen8 += g.adv8 + track8;
  }
  static uint8_t alpha[240];  // fuori dallo stack (l'ESP8266 ne ha poco)
  static uint16_t row[240];
  if (W > 240) W = 240;
  for (int r = 0; r < f.lh; r++) {
    memset(alpha, 0, W);
    const int gr = r - f.asc;  // riga relativa alla linea di base
    for (int k = 0; k < ng; k++) {
      int yy = gr - gs[k].yo;
      if (yy < 0 || yy >= gs[k].h) continue;
      const int base = yy * gs[k].w;
      for (int c = 0; c < gs[k].w; c++) {
        int px = gs[k].x + indent + c;
        if (px < 0 || px >= W) continue;
        int ni = base + c;
        uint8_t b = pgm_read_byte(gs[k].bmp + (ni >> 1));
        uint8_t a = (ni & 1) ? (b & 15) : (b >> 4);
        if (a > alpha[px]) alpha[px] = a;
      }
    }
    for (int c = 0; c < W; c++) row[c] = lut[alpha[c]];
    push(x, y + r, W, row);
  }
}

// Citazione: sceglie la dimensione piu' grande che entra nel riquadro, centra testo e autore.
// Il riquadro (x,y,w,h) deve essere gia' riempito col colore di sfondo (lut[0]).
// lutT = colori del testo, lutA = colori dell'autore.
static inline void drawQuoteBody(const char* text, const char* author, int x, int y, int w, int h,
                                 const uint16_t* lutT, const uint16_t* lutA, PushFn push) {
  const int AUTH_H = 30;  // spazio per filetto + autore
  const int areaH = h - AUTH_H;
  Line ln[12];
  int n = 0, fi = QF_TEXT_N - 1;
  for (int i = 0; i < QF_TEXT_N; i++) {
    const QFont& f = *QF_TEXT[i];
    int pitch = f.lh * 9 / 10;
    int k = layout(f, text, w, ln, 12);
    if (k > 0 && k * pitch <= areaH) { n = k; fi = i; break; }
    if (i == QF_TEXT_N - 1) {  // anche il piu' piccolo e' troppo alto: lo uso comunque
      k = layout(f, text, w, ln, 12);
      if (k > 0) n = k;
      else { n = 1; ln[0] = {0, (int16_t)strlen(text), 0}; }
    }
  }
  const QFont& f = *QF_TEXT[fi];
  const int pitch = f.lh * 9 / 10;
  const int total = n * pitch;
  int y0 = y + (areaH - total) / 2;
  if (y0 < y) y0 = y;
  for (int i = 0; i < n; i++) {
    int indent = (w * 8 - ln[i].w8) / 16;
    if (indent < 0) indent = 0;
    drawLine(f, text + ln[i].start, ln[i].len, x, y0 + i * pitch, w, lutT, push, 0, indent);
  }
  // autore, con spaziatura tra le lettere se entra
  const int alen = (int)strlen(author);
  int tr = 8;
  int aw8 = width8(QF_AUTH, author, alen, tr);
  if (aw8 > w * 8) { tr = 0; aw8 = width8(QF_AUTH, author, alen, 0); }
  int indent = (w * 8 - aw8) / 16;
  if (indent < 0) indent = 0;
  drawLine(QF_AUTH, author, alen, x, y + h - QF_AUTH.lh - 1, w, lutA, push, tr, indent);
}

}  // namespace qr
