#pragma once
/*
 * Decoder JPEG minimale (baseline / sequenziale, Huffman, 8 bit) pensato per ESP8266.
 *
 *  - legge il file in streaming (buffer da 256 byte): non serve caricare la foto in RAM
 *  - decodifica un MCU alla volta (16x16 pixel al massimo) e lo consegna a una callback
 *  - usa circa 3,5 KB di RAM, allocati solo durante la decodifica
 *  - supporta 1 componente (grigio) o 3 componenti YCbCr, campionamento 4:4:4, 4:2:2, 4:2:0, 4:4:0
 *  - NON supporta JPEG progressivi o aritmetici (i browser non li producono dal canvas)
 *
 * L'IDCT e' quella intera a 12 bit ispirata a jidctint / stb_image (dominio pubblico).
 */
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <new>

class JpegDecoder {
 public:
  typedef int (*ReadFn)(void* ctx, uint8_t* buf, int len);  // ritorna i byte letti (0 = fine)
  typedef void (*OutFn)(void* ctx, int x, int y, int w, int h, uint16_t* rgb565);
  enum { OK = 0, ERR_FORMAT = 1, ERR_UNSUPPORTED = 2, ERR_DATA = 3 };

  JpegDecoder(ReadFn rd, void* ctx) : rd_(rd), rctx_(ctx) {
    ipos_ = ilen_ = 0;
    bits_ = 0;
    nb_ = 0;
    marker_ = 0;
    w_ = h_ = 0;
    ncomp_ = 0;
    hmax_ = vmax_ = 1;
    dri_ = 0;
    memset(comp_, 0, sizeof(comp_));
    memset(qt_, 0, sizeof(qt_));
    memset(huff_, 0, sizeof(huff_));
    memset(qset_, 0, sizeof(qset_));
  }

  int width() const { return w_; }
  int height() const { return h_; }

  // Legge le intestazioni fino all'inizio dei dati. Ritorna OK se il file e' decodificabile.
  int begin() {
    if (getb() != 0xFF || getb() != 0xD8) return ERR_FORMAT;
    for (;;) {
      int b = getb();
      if (b < 0) return ERR_FORMAT;
      if (b != 0xFF) continue;
      int m;
      do { m = getb(); } while (m == 0xFF);
      if (m < 0) return ERR_FORMAT;
      if (m == 0x00 || m == 0x01 || m == 0xD8 || (m >= 0xD0 && m <= 0xD7)) continue;  // senza lunghezza
      if (m == 0xD9) return ERR_FORMAT;                                             // EOI prima dei dati
      int len = get16();
      if (len < 2) return ERR_FORMAT;
      len -= 2;
      if (m == 0xC0 || m == 0xC1) {
        int r = parseSOF(len);
        if (r != OK) return r;
      } else if (m == 0xC2 || m == 0xC3 || (m >= 0xC5 && m <= 0xC7) || (m >= 0xC9 && m <= 0xCB) ||
                 (m >= 0xCD && m <= 0xCF)) {
        return ERR_UNSUPPORTED;  // progressivo, lossless, aritmetico...
      } else if (m == 0xC4) {
        if (!parseDHT(len)) return ERR_FORMAT;
      } else if (m == 0xDB) {
        if (!parseDQT(len)) return ERR_FORMAT;
      } else if (m == 0xDD) {
        if (len != 2) return ERR_FORMAT;
        dri_ = get16();
      } else if (m == 0xDA) {
        return parseSOS(len);
      } else {
        skip(len);
      }
    }
  }

  // Decodifica l'immagine. begin() deve aver restituito OK.
  int decode(OutFn out, void* octx, bool swapRB) {
    const int mcuW = 8 * hmax_, mcuH = 8 * vmax_;
    const int mcusX = (w_ + mcuW - 1) / mcuW, mcusY = (h_ + mcuH - 1) / mcuH;
    int sx[3] = {0, 0, 0}, sy[3] = {0, 0, 0}, stride[3] = {8, 8, 8};
    for (int i = 0; i < ncomp_; i++) {
      sx[i] = (hmax_ / comp_[i].H) == 2 ? 1 : 0;
      sy[i] = (vmax_ / comp_[i].V) == 2 ? 1 : 0;
      stride[i] = 8 * comp_[i].H;
      comp_[i].pred = 0;
    }
    int count = 0;
    for (int my = 0; my < mcusY; my++) {
      for (int mx = 0; mx < mcusX; mx++) {
        if (dri_ && count && (count % dri_) == 0) {
          if (!restart()) return ERR_DATA;
        }
        for (int ci = 0; ci < ncomp_; ci++) {
          Comp& c = comp_[ci];
          for (int v = 0; v < c.V; v++) {
            for (int h = 0; h < c.H; h++) {
              if (!decodeBlock(c)) return ERR_DATA;
              idct(plane_[ci] + (v * 8) * stride[ci] + h * 8, stride[ci]);
            }
          }
        }
        // conversione colore dell'MCU
        const int x0 = mx * mcuW, y0 = my * mcuH;
        const int bw = (w_ - x0 < mcuW) ? (w_ - x0) : mcuW;
        const int bh = (h_ - y0 < mcuH) ? (h_ - y0) : mcuH;
        uint16_t* o = out_;
        for (int py = 0; py < bh; py++) {
          const uint8_t* yr = plane_[0] + (py >> sy[0]) * stride[0];
          if (ncomp_ == 1) {
            for (int px = 0; px < bw; px++) {
              int y = yr[px >> sx[0]];
              *o++ = pack(y, y, y, false);
            }
          } else {
            const uint8_t* br = plane_[1] + (py >> sy[1]) * stride[1];
            const uint8_t* rr = plane_[2] + (py >> sy[2]) * stride[2];
            for (int px = 0; px < bw; px++) {
              int y = yr[px >> sx[0]];
              int cb = br[px >> sx[1]] - 128;
              int cr = rr[px >> sx[2]] - 128;
              int r = y + ((91881 * cr + 32768) >> 16);
              int g = y - ((22554 * cb + 46802 * cr + 32768) >> 16);
              int b = y + ((116130 * cb + 32768) >> 16);
              *o++ = pack(r, g, b, swapRB);
            }
          }
        }
        out(octx, x0, y0, bw, bh, out_);
        count++;
      }
    }
    return OK;
  }

 private:
  struct Comp {
    uint8_t id, H, V, tq, td, ta;
    int pred;
  };
  struct Huff {
    bool set;
    uint8_t vals[256];
    int mincode[17];
    int maxcode[17];
    int valptr[17];
  };

  ReadFn rd_;
  void* rctx_;
  uint8_t ibuf_[256];
  int ipos_, ilen_;
  uint32_t bits_;
  int nb_;
  int marker_;  // marcatore incontrato nei dati (0 = nessuno)

  int w_, h_, ncomp_, hmax_, vmax_, dri_;
  Comp comp_[3];
  uint16_t qt_[4][64];  // in ordine zig-zag
  bool qset_[4];
  Huff huff_[4];  // 0,1 = DC ; 2,3 = AC
  uint8_t plane_[3][256];
  uint16_t out_[256];
  int blk_[64];

  static const uint8_t* zz() {
    static const uint8_t z[64] = {0,  1,  8,  16, 9,  2,  3,  10, 17, 24, 32, 25, 18, 11, 4,  5,
                                  12, 19, 26, 33, 40, 48, 41, 34, 27, 20, 13, 6,  7,  14, 21, 28,
                                  35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51,
                                  58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63};
    return z;
  }

  // ---------------- lettura ----------------
  int getb() {
    if (ipos_ >= ilen_) {
      ilen_ = rd_(rctx_, ibuf_, (int)sizeof(ibuf_));
      ipos_ = 0;
      if (ilen_ <= 0) {
        ilen_ = 0;
        return -1;
      }
    }
    return ibuf_[ipos_++];
  }
  int get16() {
    int a = getb(), b = getb();
    if (a < 0 || b < 0) return -1;
    return (a << 8) | b;
  }
  void skip(int n) {
    while (n-- > 0) {
      if (getb() < 0) break;
    }
  }

  // ---------------- intestazioni ----------------
  int parseSOF(int len) {
    if (len < 6) return ERR_FORMAT;
    int prec = getb();
    h_ = get16();
    w_ = get16();
    int n = getb();
    if (prec != 8) return ERR_UNSUPPORTED;
    if (w_ <= 0 || h_ <= 0 || w_ > 2048 || h_ > 2048) return ERR_UNSUPPORTED;
    if ((n != 1 && n != 3) || len != 6 + 3 * n) return ERR_UNSUPPORTED;
    ncomp_ = n;
    hmax_ = vmax_ = 1;
    for (int i = 0; i < n; i++) {
      comp_[i].id = (uint8_t)getb();
      int hv = getb();
      comp_[i].tq = (uint8_t)getb();
      comp_[i].H = (uint8_t)(hv >> 4);
      comp_[i].V = (uint8_t)(hv & 15);
      if (comp_[i].H < 1 || comp_[i].H > 2 || comp_[i].V < 1 || comp_[i].V > 2 || comp_[i].tq > 3) return ERR_UNSUPPORTED;
      if (comp_[i].H > hmax_) hmax_ = comp_[i].H;
      if (comp_[i].V > vmax_) vmax_ = comp_[i].V;
    }
    if (n == 1) {  // un solo componente: si decodifica come blocchi 8x8
      comp_[0].H = comp_[0].V = 1;
      hmax_ = vmax_ = 1;
    }
    return OK;
  }

  bool parseDQT(int len) {
    while (len > 0) {
      int pt = getb();
      len--;
      int prec = pt >> 4, id = pt & 15;
      if (pt < 0 || id > 3 || prec > 1) return false;
      for (int i = 0; i < 64; i++) {
        int v;
        if (prec) {
          v = get16();
          len -= 2;
        } else {
          v = getb();
          len -= 1;
        }
        if (v < 0) return false;
        qt_[id][i] = (uint16_t)v;
      }
      qset_[id] = true;
    }
    return len == 0;
  }

  bool parseDHT(int len) {
    while (len > 0) {
      int tc = getb();
      if (tc < 0) return false;
      int cls = tc >> 4, id = tc & 15;
      if (cls > 1 || id > 1) return false;  // solo tabelle baseline (0 e 1)
      uint8_t counts[17];
      int total = 0;
      for (int i = 1; i <= 16; i++) {
        int c = getb();
        if (c < 0) return false;
        counts[i] = (uint8_t)c;
        total += c;
      }
      len -= 17;
      if (total > 256 || total > len) return false;
      Huff& h = huff_[cls * 2 + id];
      for (int i = 0; i < total; i++) {
        int v = getb();
        if (v < 0) return false;
        h.vals[i] = (uint8_t)v;
      }
      len -= total;
      int code = 0, k = 0;
      for (int l = 1; l <= 16; l++) {
        h.valptr[l] = k;
        h.mincode[l] = code;
        code += counts[l];
        k += counts[l];
        h.maxcode[l] = counts[l] ? code - 1 : -1;
        code <<= 1;
      }
      h.set = true;
    }
    return len == 0;
  }

  int parseSOS(int len) {
    if (ncomp_ == 0) return ERR_FORMAT;
    int ns = getb();
    if (ns != ncomp_ || len != 1 + 2 * ns + 3) return ERR_UNSUPPORTED;  // solo scansione unica
    for (int i = 0; i < ns; i++) {
      int cid = getb();
      int t = getb();
      int k = -1;
      for (int j = 0; j < ncomp_; j++) {
        if (comp_[j].id == cid) k = j;
      }
      if (k < 0 || t < 0) return ERR_FORMAT;
      comp_[k].td = (uint8_t)(t >> 4);
      comp_[k].ta = (uint8_t)(t & 15);
    }
    skip(3);  // Ss, Se, Ah/Al
    for (int j = 0; j < ncomp_; j++) {
      Comp& c = comp_[j];
      if (c.td > 1 || c.ta > 1 || !huff_[c.td].set || !huff_[2 + c.ta].set || !qset_[c.tq]) return ERR_FORMAT;
    }
    return OK;
  }

  // ---------------- dati entropici ----------------
  int entropyByte() {
    if (marker_) return 0;
    int b = getb();
    if (b < 0) {
      marker_ = 0xD9;
      return 0;
    }
    if (b != 0xFF) return b;
    int b2 = getb();
    while (b2 == 0xFF) b2 = getb();
    if (b2 == 0) return 0xFF;
    marker_ = (b2 < 0) ? 0xD9 : b2;
    return 0;
  }
  void need(int n) {
    while (nb_ < n) {
      bits_ = (bits_ << 8) | (uint32_t)entropyByte();
      nb_ += 8;
    }
  }
  int getBit() {
    if (nb_ == 0) need(1);
    nb_--;
    return (int)((bits_ >> nb_) & 1);
  }
  int receive(int n) {
    if (n == 0) return 0;
    need(n);
    nb_ -= n;
    return (int)((bits_ >> nb_) & ((1u << n) - 1));
  }
  static int extend(int v, int t) { return (v < (1 << (t - 1))) ? v - (1 << t) + 1 : v; }

  int huffDecode(const Huff& h) {
    int code = 0;
    for (int l = 1; l <= 16; l++) {
      code = (code << 1) | getBit();
      if (code <= h.maxcode[l]) return h.vals[h.valptr[l] + code - h.mincode[l]];
    }
    return -1;
  }

  bool restart() {
    bits_ = 0;
    nb_ = 0;
    if (!marker_) {
      for (;;) {
        int b;
        do { b = getb(); } while (b >= 0 && b != 0xFF);
        if (b < 0) return false;
        int m;
        do { m = getb(); } while (m == 0xFF);
        if (m < 0) return false;
        if (m != 0) {
          marker_ = m;
          break;
        }
      }
    }
    if (marker_ < 0xD0 || marker_ > 0xD7) return false;
    marker_ = 0;
    for (int i = 0; i < ncomp_; i++) comp_[i].pred = 0;
    return true;
  }

  bool decodeBlock(Comp& c) {
    memset(blk_, 0, sizeof(blk_));
    const uint16_t* q = qt_[c.tq];
    int t = huffDecode(huff_[c.td]);
    if (t < 0 || t > 11) return false;
    int diff = t ? extend(receive(t), t) : 0;
    c.pred += diff;
    blk_[0] = c.pred * (int)q[0];
    const Huff& ac = huff_[2 + c.ta];
    const uint8_t* z = zz();
    for (int k = 1; k < 64;) {
      int rs = huffDecode(ac);
      if (rs < 0) return false;
      int r = rs >> 4, s = rs & 15;
      if (s == 0) {
        if (r == 15) {
          k += 16;
          continue;
        }
        break;
      }
      k += r;
      if (k > 63) return false;
      blk_[z[k]] = extend(receive(s), s) * (int)q[k];
      k++;
    }
    return true;
  }

  // ---------------- IDCT ----------------
  static inline uint8_t clamp8(int x) { return ((unsigned)x > 255u) ? (x < 0 ? 0 : 255) : (uint8_t)x; }
#define JD_F2F(x) ((int)(((x) * 4096 + 0.5)))
#define JD_FSH(x) ((x) * 4096)
#define JD_IDCT_1D(s0, s1, s2, s3, s4, s5, s6, s7)                              \
  int t0, t1, t2, t3, p1, p2, p3, p4, p5, x0, x1, x2, x3;                       \
  p2 = s2;                                                                      \
  p3 = s6;                                                                      \
  p1 = (p2 + p3) * JD_F2F(0.5411961f);                                          \
  t2 = p1 + p3 * JD_F2F(-1.847759065f);                                         \
  t3 = p1 + p2 * JD_F2F(0.765366865f);                                          \
  p2 = s0;                                                                      \
  p3 = s4;                                                                      \
  t0 = JD_FSH(p2 + p3);                                                         \
  t1 = JD_FSH(p2 - p3);                                                         \
  x0 = t0 + t3;                                                                 \
  x3 = t0 - t3;                                                                 \
  x1 = t1 + t2;                                                                 \
  x2 = t1 - t2;                                                                 \
  t0 = s7;                                                                      \
  t1 = s5;                                                                      \
  t2 = s3;                                                                      \
  t3 = s1;                                                                      \
  p3 = t0 + t2;                                                                 \
  p4 = t1 + t3;                                                                 \
  p1 = t0 + t3;                                                                 \
  p2 = t1 + t2;                                                                 \
  p5 = (p3 + p4) * JD_F2F(1.175875602f);                                        \
  t0 = t0 * JD_F2F(0.298631336f);                                               \
  t1 = t1 * JD_F2F(2.053119869f);                                               \
  t2 = t2 * JD_F2F(3.072711026f);                                               \
  t3 = t3 * JD_F2F(1.501321110f);                                               \
  p1 = p5 + p1 * JD_F2F(-0.899976223f);                                         \
  p2 = p5 + p2 * JD_F2F(-2.562915447f);                                         \
  p3 = p3 * JD_F2F(-1.961570560f);                                              \
  p4 = p4 * JD_F2F(-0.390180644f);                                              \
  t3 += p1 + p4;                                                                \
  t2 += p2 + p3;                                                                \
  t1 += p2 + p4;                                                                \
  t0 += p1 + p3;

  void idct(uint8_t* out, int stride) {
    int val[64];
    int* v = val;
    int* d = blk_;
    for (int i = 0; i < 8; ++i, ++d, ++v) {  // colonne
      if (d[8] == 0 && d[16] == 0 && d[24] == 0 && d[32] == 0 && d[40] == 0 && d[48] == 0 && d[56] == 0) {
        int dc = d[0] * 4;
        v[0] = v[8] = v[16] = v[24] = v[32] = v[40] = v[48] = v[56] = dc;
      } else {
        JD_IDCT_1D(d[0], d[8], d[16], d[24], d[32], d[40], d[48], d[56])
        x0 += 512;
        x1 += 512;
        x2 += 512;
        x3 += 512;
        v[0] = (x0 + t3) >> 10;
        v[56] = (x0 - t3) >> 10;
        v[8] = (x1 + t2) >> 10;
        v[48] = (x1 - t2) >> 10;
        v[16] = (x2 + t1) >> 10;
        v[40] = (x2 - t1) >> 10;
        v[24] = (x3 + t0) >> 10;
        v[32] = (x3 - t0) >> 10;
      }
    }
    uint8_t* o = out;
    v = val;
    for (int i = 0; i < 8; ++i, v += 8, o += stride) {  // righe
      JD_IDCT_1D(v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7])
      x0 += 65536 + (128 << 17);
      x1 += 65536 + (128 << 17);
      x2 += 65536 + (128 << 17);
      x3 += 65536 + (128 << 17);
      o[0] = clamp8((x0 + t3) >> 17);
      o[7] = clamp8((x0 - t3) >> 17);
      o[1] = clamp8((x1 + t2) >> 17);
      o[6] = clamp8((x1 - t2) >> 17);
      o[2] = clamp8((x2 + t1) >> 17);
      o[5] = clamp8((x2 - t1) >> 17);
      o[3] = clamp8((x3 + t0) >> 17);
      o[4] = clamp8((x3 - t0) >> 17);
    }
  }

  // ---------------- colore ----------------
  static inline uint16_t pack(int r, int g, int b, bool swapRB) {
    uint8_t R = clamp8(r), G = clamp8(g), B = clamp8(b);
    if (swapRB) {
      uint8_t t = R;
      R = B;
      B = t;
    }
    return (uint16_t)(((R & 0xF8) << 8) | ((G & 0xFC) << 3) | (B >> 3));
  }
};
