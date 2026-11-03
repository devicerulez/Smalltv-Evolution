/*
 * SmallTV Ultra - Orologio + Meteo
 * Hardware: ESP8266 (4 MB flash) + TFT ST7789 240x240
 *
 *  - Portale Wi-Fi (captive portal) al primo avvio o se la rete non si collega
 *  - Ora automatica via NTP, fuso Europe/Rome con ora legale
 *  - Meteo da Open-Meteo (gratuito, nessuna chiave API)
 *  - Interfaccia web di configurazione: http://smalltv.local  (oppure l'IP mostrato allo schermo)
 *  - Aggiornamento firmware da browser: /update
 */

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266mDNS.h>
#include <WiFiClientSecure.h>
#include <DNSServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <TFT_eSPI.h>
#include <time.h>
#include "webui.h"
#include "jpegdec.h"
#include "moon.h"
#include "moondraw.h"
#include "quotes.h"
#include "qrender.h"

#define FW_VERSION  "1.0.3"
#define HOSTNAME    "smalltv"
#define AP_SSID     "SmallTV-Setup"
#define AP_PASS     "smalltv1234"
#define TZ_ROME     "CET-1CEST,M3.5.0,M10.5.0/3"

#ifndef BL_PIN
#define BL_PIN 5
#endif
#ifndef BL_ACTIVE_LOW
#define BL_ACTIVE_LOW 1
#endif

// ------------------------------------------------------------------
//  Layout (schermo 240x240)
// ------------------------------------------------------------------
static const int HEADER_H = 28;
static const int TIME_Y   = 28;    // top del testo dell'ora (font 8, alto 75 px)
static const int BAR_Y    = 107;   // barra dei secondi
static const int CARD_X   = 6;
static const int CARD_Y   = 116;
static const int CARD_W   = 228;
static const int CARD_H   = 122;
static const int DESC_Y   = 188;   // riga delle condizioni (a rotazione)
static const uint32_t INFO_ROTATE_MS = 4000;  // cambio dato ogni 4 secondi
static const int MAX_PHOTOS = 20;              // foto memorizzabili (file /ph0.jpg ... /ph19.jpg)
static const size_t MAX_PHOTO_BYTES = 28 * 1024;  // dimensione massima di una foto

// ------------------------------------------------------------------
//  Oggetti globali
// ------------------------------------------------------------------
TFT_eSPI tft = TFT_eSPI();
ESP8266WebServer server(80);
ESP8266HTTPUpdateServer httpUpdater;
DNSServer dns;

// ------------------------------------------------------------------
//  Configurazione persistente
// ------------------------------------------------------------------
struct Config {
  char ssid[33];
  char pass[65];
  char city[24];
  float lat;
  float lon;
  uint8_t theme;
  uint8_t bright;     // 5..100
  uint8_t wxMinutes;  // intervallo aggiornamento meteo
  bool flip;          // ruota 180 gradi
  bool invert;        // inverti colori pannello
  bool swapRB;        // scambia rosso/blu
  bool h12;           // formato orario 12 h (AM/PM)
  bool nightOn;       // modalita' notturna attiva
  uint16_t nStart;    // inizio notte (minuti dalla mezzanotte)
  uint16_t nEnd;      // fine notte (minuti dalla mezzanotte)
  uint8_t nBright;    // luminosita' notturna 0..100
  bool fcOn;          // schermata previsioni dei prossimi 3 giorni
  uint8_t fcSec;      // durata previsioni (secondi)
  bool phOn;          // schermata foto
  uint8_t phSec;      // durata di ogni foto (secondi)
  bool moOn;          // schermata fasi lunari
  uint8_t moSec;      // durata fasi lunari (secondi)
  bool qtOn;          // schermata citazione del giorno
  uint8_t qtSec;      // durata citazione (secondi)
};
Config cfg;

void cfgDefaults() {
  memset(&cfg, 0, sizeof(cfg));
  strlcpy(cfg.city, "ROMA", sizeof(cfg.city));
  cfg.lat = 41.8919f;
  cfg.lon = 12.5113f;
  cfg.theme = 0;
  cfg.bright = 80;
  cfg.wxMinutes = 15;
  cfg.flip = false;
  cfg.invert = true;
  cfg.swapRB = false;
  cfg.h12 = false;
  cfg.nightOn = false;
  cfg.nStart = 22 * 60;
  cfg.nEnd = 7 * 60;
  cfg.nBright = 10;
  cfg.fcOn = false;
  cfg.fcSec = 5;
  cfg.phOn = false;
  cfg.phSec = 5;
  cfg.moOn = false;
  cfg.moSec = 5;
  cfg.qtOn = false;
  cfg.qtSec = 5;
}

// copia solo caratteri ASCII stampabili (i font del display sono ASCII)
void asciiCopy(char* dst, size_t n, const char* src) {
  size_t j = 0;
  for (size_t i = 0; src && src[i] && j < n - 1; i++) {
    uint8_t c = (uint8_t)src[i];
    if (c >= 32 && c < 127) dst[j++] = (char)c;
  }
  dst[j] = 0;
}

// ------------------------------------------------------------------
//  Temi (0xRRGGBB)
// ------------------------------------------------------------------
struct Theme {
  uint32_t bg, card, a1, a2, text, dim, band;
};
const Theme THEMES[] = {
  // bg        card      a1        a2        text      dim       band
  {0x05060F, 0x0E1230, 0x00E5FF, 0xFF2E93, 0xFFFFFF, 0x6B7399, 0x2A1470},  // Neon
  {0x030F0E, 0x0A2422, 0x2DFFB2, 0x7C5CFF, 0xEFFFFA, 0x5F8F86, 0x0B4A48},  // Aurora
  {0x120610, 0x2A0F26, 0xFF9F1C, 0xFF3D81, 0xFFF4E6, 0x9C6B7E, 0x6A1A4A},  // Tramonto
  {0x06101C, 0x0F2338, 0x7FD1FF, 0xA78BFA, 0xF2F8FF, 0x6F8CAB, 0x14487A},  // Ghiaccio
  {0x000000, 0x14100A, 0xFFB000, 0xFF6A00, 0xFFE9B8, 0x7A5C1E, 0x4A3400},  // Ambra
};
const uint8_t THEME_COUNT = sizeof(THEMES) / sizeof(THEMES[0]);
const Theme* th = &THEMES[0];

// ------------------------------------------------------------------
//  Colori
// ------------------------------------------------------------------
uint16_t col(uint32_t rgb) {
  uint8_t r = (rgb >> 16) & 0xFF, g = (rgb >> 8) & 0xFF, b = rgb & 0xFF;
  return cfg.swapRB ? tft.color565(b, g, r) : tft.color565(r, g, b);
}

// t=0 -> a, t=255 -> b
uint32_t mixRGB(uint32_t a, uint32_t b, uint8_t t) {
  int ar = (a >> 16) & 255, ag = (a >> 8) & 255, ab = a & 255;
  int br = (b >> 16) & 255, bg = (b >> 8) & 255, bb = b & 255;
  int r = (ar * (255 - t) + br * t) / 255;
  int g = (ag * (255 - t) + bg * t) / 255;
  int bl = (ab * (255 - t) + bb * t) / 255;
  return ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)bl;
}

// ------------------------------------------------------------------
//  Stato
// ------------------------------------------------------------------
bool apMode = false;
bool needRedraw = false;
bool wxDirty = true;
bool pendingReboot = false;
uint32_t rebootAt = 0;
uint32_t apStart = 0;
uint32_t apLastClient = 0;
uint32_t nextWx = 0;

struct Weather {
  bool valid;
  float temp;
  int hum;
  int code;
  bool day;
  float tmin;
  float tmax;
  uint32_t fetchedAt;
  float feels;    // temperatura percepita
  float wind;     // km/h
  float pres;     // hPa (livello del mare)
  int sunrise;    // minuti dalla mezzanotte, -1 se non disponibile
  int sunset;
  bool fcValid;   // previsioni dei 3 giorni successivi disponibili
  int fcCode[3];
  float fcMax[3];
  float fcMin[3];
};
Weather wx = {false, 0, 0, 0, true, 0, 0, 0, 0, 0, 0, -1, -1, false, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}};

uint8_t timeFont = 8;
int timeY = TIME_Y;

time_t lastNow = 0;
int lastDigitsKey = -1;
int lastHdrKey = -1000;
int lastSec = -1;
uint8_t infoIdx = 0;          // quale dato mostrare nella riga delle condizioni
uint32_t lastInfoRot = 0;
int curBright = -1;           // luminosita' applicata (-1 = ancora nessuna)
uint32_t lastBrightCheck = 0;

const char* const DAYS[] = {"DOM", "LUN", "MAR", "MER", "GIO", "VEN", "SAB"};
const char* const MONTHS[] = {"GEN", "FEB", "MAR", "APR", "MAG", "GIU", "LUG", "AGO", "SET", "OTT", "NOV", "DIC"};

// ------------------------------------------------------------------
//  File di configurazione
// ------------------------------------------------------------------
bool loadConfig() {
  cfgDefaults();
  File f = LittleFS.open("/config.json", "r");
  if (!f) return false;
  DynamicJsonDocument d(1024);
  DeserializationError e = deserializeJson(d, f);
  f.close();
  if (e) return false;

  strlcpy(cfg.ssid, d["ssid"] | "", sizeof(cfg.ssid));
  strlcpy(cfg.pass, d["pass"] | "", sizeof(cfg.pass));
  asciiCopy(cfg.city, sizeof(cfg.city), d["city"] | "ROMA");
  if (cfg.city[0] == 0) strlcpy(cfg.city, "ROMA", sizeof(cfg.city));
  cfg.lat = d["lat"] | 41.8919f;
  cfg.lon = d["lon"] | 12.5113f;
  int v = d["theme"] | 0;
  cfg.theme = constrain(v, 0, (int)THEME_COUNT - 1);
  v = d["bright"] | 80;
  cfg.bright = constrain(v, 5, 100);
  v = d["wx"] | 15;
  cfg.wxMinutes = constrain(v, 5, 120);
  cfg.flip = d["flip"] | false;
  cfg.invert = d["invert"] | true;
  cfg.swapRB = d["swap"] | false;
  cfg.h12 = d["h12"] | false;
  cfg.nightOn = d["night"] | false;
  v = d["ns"] | (22 * 60);
  cfg.nStart = constrain(v, 0, 1439);
  v = d["ne"] | (7 * 60);
  cfg.nEnd = constrain(v, 0, 1439);
  v = d["nb"] | 10;
  cfg.nBright = constrain(v, 0, 100);
  cfg.fcOn = d["fc"] | false;
  v = d["fcs"] | 5;
  cfg.fcSec = constrain(v, 2, 60);
  cfg.phOn = d["ph"] | false;
  v = d["phs"] | 5;
  cfg.phSec = constrain(v, 2, 60);
  cfg.moOn = d["mo"] | false;
  v = d["mos"] | 5;
  cfg.moSec = constrain(v, 2, 60);
  cfg.qtOn = d["qt"] | false;
  v = d["qts"] | 5;
  cfg.qtSec = constrain(v, 2, 60);
  return true;
}

bool saveConfig() {
  DynamicJsonDocument d(1024);
  d["ssid"] = cfg.ssid;
  d["pass"] = cfg.pass;
  d["city"] = cfg.city;
  d["lat"] = cfg.lat;
  d["lon"] = cfg.lon;
  d["theme"] = cfg.theme;
  d["bright"] = cfg.bright;
  d["wx"] = cfg.wxMinutes;
  d["flip"] = cfg.flip;
  d["invert"] = cfg.invert;
  d["swap"] = cfg.swapRB;
  d["h12"] = cfg.h12;
  d["night"] = cfg.nightOn;
  d["ns"] = cfg.nStart;
  d["ne"] = cfg.nEnd;
  d["nb"] = cfg.nBright;
  d["fc"] = cfg.fcOn;
  d["fcs"] = cfg.fcSec;
  d["ph"] = cfg.phOn;
  d["phs"] = cfg.phSec;
  d["mo"] = cfg.moOn;
  d["mos"] = cfg.moSec;
  d["qt"] = cfg.qtOn;
  d["qts"] = cfg.qtSec;
  File f = LittleFS.open("/config.json", "w");
  if (!f) return false;
  serializeJson(d, f);
  f.close();
  return true;
}

// ------------------------------------------------------------------
//  Retroilluminazione
// ------------------------------------------------------------------
void setBacklight(uint8_t pct) {
  int p = constrain((int)pct, 0, 100);
  int duty = map(p, 0, 100, 0, 255);
#if BL_ACTIVE_LOW
  duty = 255 - duty;
#endif
  analogWrite(BL_PIN, duty);
}

// true se l'ora attuale cade nella fascia notturna impostata
bool nightActive() {
  if (!cfg.nightOn || cfg.nStart == cfg.nEnd) return false;
  time_t now = time(nullptr);
  if (now < 1700000000) return false;  // ora non ancora sincronizzata
  struct tm ti;
  localtime_r(&now, &ti);
  int m = ti.tm_hour * 60 + ti.tm_min;
  if (cfg.nStart < cfg.nEnd) return m >= cfg.nStart && m < cfg.nEnd;
  return m >= cfg.nStart || m < cfg.nEnd;  // fascia a cavallo della mezzanotte
}

// applica la luminosita' di giorno o di notte (solo se cambiata, oppure con force)
void applyBrightness(bool force) {
  int target = nightActive() ? cfg.nBright : cfg.bright;
  if (force || target != curBright) {
    curBright = target;
    setBacklight((uint8_t)target);
  }
}

// ------------------------------------------------------------------
//  Icone meteo (disegnate con primitive, nessuna immagine)
// ------------------------------------------------------------------
enum Icon { IC_SUN, IC_MOON, IC_PARTLY, IC_PARTLY_NIGHT, IC_CLOUD, IC_FOG, IC_RAIN, IC_SNOW, IC_STORM };

Icon iconFor(int code, bool day) {
  if (code == 0 || code == 1) return day ? IC_SUN : IC_MOON;
  if (code == 2) return day ? IC_PARTLY : IC_PARTLY_NIGHT;
  if (code == 3) return IC_CLOUD;
  if (code == 45 || code == 48) return IC_FOG;
  if ((code >= 51 && code <= 67) || (code >= 80 && code <= 82)) return IC_RAIN;
  if ((code >= 71 && code <= 77) || code == 85 || code == 86) return IC_SNOW;
  if (code >= 95) return IC_STORM;
  return IC_CLOUD;
}

const char* descFor(int c) {
  switch (c) {
    case 0: return "Sereno";
    case 1: return "Quasi sereno";
    case 2: return "Parz. nuvoloso";
    case 3: return "Coperto";
    case 45: case 48: return "Nebbia";
    case 51: case 53: case 55: return "Pioviggine";
    case 56: case 57: return "Pioggia gelata";
    case 61: return "Pioggia debole";
    case 63: return "Pioggia";
    case 65: return "Pioggia forte";
    case 66: case 67: return "Pioggia gelata";
    case 71: return "Neve debole";
    case 73: return "Neve";
    case 75: return "Neve forte";
    case 77: return "Nevischio";
    case 80: case 81: return "Rovesci";
    case 82: return "Rovesci forti";
    case 85: case 86: return "Neve a rovesci";
    case 95: return "Temporale";
    case 96: case 99: return "Grandine";
    default: return "--";
  }
}

void drawSunShape(int cx, int cy, int r, uint16_t c) {
  tft.fillCircle(cx, cy, r, c);
  for (int i = 0; i < 8; i++) {
    float a = i * PI / 4.0f;
    float ca = cosf(a), sa = sinf(a);
    int x1 = cx + (int)(ca * (r + 4)), y1 = cy + (int)(sa * (r + 4));
    int x2 = cx + (int)(ca * (r + 10)), y2 = cy + (int)(sa * (r + 10));
    tft.drawLine(x1, y1, x2, y2, c);
    tft.drawLine(x1 + 1, y1, x2 + 1, y2, c);
    tft.drawLine(x1, y1 + 1, x2, y2 + 1, c);
  }
}

void drawMoonShape(int cx, int cy, int r, uint16_t c, uint16_t bgc) {
  tft.fillCircle(cx, cy, r, c);
  tft.fillCircle(cx + r / 2 + 1, cy - r / 3, r - 2, bgc);
}

void drawCloudShape(int cx, int cy, uint16_t c) {
  tft.fillRoundRect(cx - 22, cy + 1, 46, 15, 7, c);
  tft.fillCircle(cx - 7, cy - 2, 10, c);
  tft.fillCircle(cx + 7, cy - 6, 13, c);
}

void drawWeatherIcon(Icon ic, int cx, int cy, uint16_t cardc) {
  const uint16_t sun = col(0xFFC531), moon = col(0xE8EEFF), cl = col(0xD3DEF0), dk = col(0x93A5BF);
  const uint16_t rn = col(0x4FB8FF), sn = col(0xFFFFFF), bolt = col(0xFFE14A), fog = col(0xB5C3D8);
  switch (ic) {
    case IC_SUN:
      drawSunShape(cx, cy, 13, sun);
      break;
    case IC_MOON:
      drawMoonShape(cx - 2, cy, 17, moon, cardc);
      tft.drawFastHLine(cx + 14, cy - 14, 5, moon);
      tft.drawFastVLine(cx + 16, cy - 16, 5, moon);
      tft.fillCircle(cx + 18, cy + 8, 1, moon);
      break;
    case IC_PARTLY:
      drawSunShape(cx + 8, cy - 9, 9, sun);
      drawCloudShape(cx - 4, cy + 6, cl);
      break;
    case IC_PARTLY_NIGHT:
      drawMoonShape(cx + 8, cy - 9, 12, moon, cardc);
      drawCloudShape(cx - 4, cy + 6, cl);
      break;
    case IC_CLOUD:
      drawCloudShape(cx, cy, cl);
      break;
    case IC_FOG:
      drawCloudShape(cx, cy - 10, cl);
      tft.fillRoundRect(cx - 22, cy + 8, 44, 4, 2, fog);
      tft.fillRoundRect(cx - 16, cy + 15, 40, 4, 2, fog);
      tft.fillRoundRect(cx - 22, cy + 22, 34, 4, 2, fog);
      break;
    case IC_RAIN:
      drawCloudShape(cx, cy - 8, dk);
      for (int i = 0; i < 4; i++) {
        int x = cx - 14 + i * 10;
        int y = cy + 14 + ((i & 1) ? 5 : 0);
        tft.drawLine(x, y, x - 3, y + 8, rn);
        tft.drawLine(x + 1, y, x - 2, y + 8, rn);
      }
      break;
    case IC_SNOW:
      drawCloudShape(cx, cy - 8, dk);
      tft.fillCircle(cx - 14, cy + 15, 2, sn);
      tft.fillCircle(cx - 4, cy + 23, 2, sn);
      tft.fillCircle(cx + 6, cy + 15, 2, sn);
      tft.fillCircle(cx + 16, cy + 23, 2, sn);
      break;
    case IC_STORM:
      drawCloudShape(cx, cy - 10, dk);
      tft.fillTriangle(cx + 5, cy + 5, cx - 6, cy + 17, cx + 2, cy + 17, bolt);
      tft.fillTriangle(cx - 1, cy + 13, cx + 9, cy + 13, cx - 4, cy + 26, bolt);
      break;
  }
}

// ------------------------------------------------------------------
//  Disegno schermate
// ------------------------------------------------------------------
int wifiLevel() {
  if (apMode || WiFi.status() != WL_CONNECTED) return 0;
  int r = WiFi.RSSI();
  if (r > -55) return 4;
  if (r > -65) return 3;
  if (r > -75) return 2;
  return 1;
}

void drawBand() {
  for (int y = 0; y < HEADER_H - 1; y++) {
    uint8_t t = (uint8_t)(y * 255 / (HEADER_H - 2));
    tft.drawFastHLine(0, y, 240, col(mixRGB(th->band, th->bg, t)));
  }
  tft.drawFastHLine(0, HEADER_H - 1, 240, col(mixRGB(th->bg, th->a1, 110)));
}

void drawHeader(const struct tm* ti, bool synced) {
  drawBand();
  tft.setTextDatum(TL_DATUM);
  if (synced && ti) {
    tft.setTextColor(col(th->a1));
    tft.drawString(DAYS[ti->tm_wday], 10, 1, 4);
    int w = tft.textWidth(DAYS[ti->tm_wday], 4);
    char buf[16];
    snprintf(buf, sizeof(buf), " %d %s", ti->tm_mday, MONTHS[ti->tm_mon]);
    tft.setTextColor(col(th->text));
    tft.drawString(buf, 10 + w, 1, 4);
    // anno dopo il mese; se non entra prima dell'icona Wi-Fi uso un font piu' piccolo
    int x = 10 + w + tft.textWidth(buf, 4);
    char yb[8];
    snprintf(yb, sizeof(yb), " %d", ti->tm_year + 1900);
    int yf = (x + tft.textWidth(yb, 4) <= 200) ? 4 : 2;
    tft.drawString(yb, x, (yf == 4) ? 1 : 7, yf);
  } else {
    tft.setTextColor(col(th->dim));
    tft.drawString("Sincronizzo ora...", 10, 6, 2);
  }
  int lvl = wifiLevel();
  for (int i = 0; i < 4; i++) {
    int h = 5 + i * 4;
    int x = 204 + i * 7;
    uint32_t c = (i < lvl) ? th->text : mixRGB(th->band, th->dim, 120);
    tft.fillRect(x, 22 - h, 4, h, col(c));
  }
}

void initTimeFont() {
  int wD = tft.textWidth("00", 8);
  int wC = tft.textWidth(":", 8);
  if (wD > 0 && (2 * wD + wC) <= 228) {
    timeFont = 8;
    timeY = TIME_Y;
  } else {  // il font 8 non entra: ripiego sul 7 segmenti
    timeFont = 7;
    timeY = TIME_Y + 14;
  }
}

void timeGeom(int& x0, int& wD, int& wC) {
  wD = tft.textWidth("00", timeFont);
  wC = tft.textWidth(":", timeFont);
  x0 = (240 - (2 * wD + wC)) / 2;
}

void drawTimeDigits(int hh, int mm, bool synced) {
  int x0, wD, wC;
  timeGeom(x0, wD, wC);
  uint16_t bg = col(th->bg);
  const bool use12 = cfg.h12 && synced;
  int dh = hh;
  if (use12) {
    dh = hh % 12;
    if (dh == 0) dh = 12;
  }
  char b[4];
  tft.setTextDatum(TL_DATUM);
  snprintf(b, sizeof(b), "%02d", dh);
  tft.setTextColor(col(synced ? th->text : th->dim), bg);
  tft.drawString(b, x0, timeY, timeFont);
  snprintf(b, sizeof(b), "%02d", mm);
  tft.setTextColor(col(synced ? th->a1 : th->dim), bg);
  tft.drawString(b, x0 + wD + wC, timeY, timeFont);

  // indicatore AM / PM a destra dei minuti (solo nel formato 12 ore)
  if (use12) {
    int wAp = tft.textWidth("PM", 4);
    int ax = x0 + 2 * wD + wC + 6;
    if (ax + wAp > 238) ax = 238 - wAp;
    int ay = timeY + tft.fontHeight(timeFont) - 28;
    tft.setTextColor(col(th->a2), bg);
    tft.setTextPadding(wAp);
    tft.drawString((hh >= 12) ? "PM" : "AM", ax, ay, 4);
    tft.setTextPadding(0);
  }
}

void drawColon(bool on) {
  int x0, wD, wC;
  timeGeom(x0, wD, wC);
  uint16_t bg = col(th->bg);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(on ? col(th->a2) : bg, bg);
  tft.drawString(":", x0 + wD, timeY, timeFont);
}

void drawSecBar(int sec) {
  int w = (sec + 1) * 224 / 60;
  tft.fillRect(8, BAR_Y, w, 4, col(th->a1));
  tft.fillRect(8 + w, BAR_Y, 224 - w, 4, col(mixRGB(th->bg, th->dim, 70)));
  if (w > 6) tft.fillRect(8 + w - 6, BAR_Y, 6, 4, col(th->a2));
}

// ------------------------------------------------------------------
//  Card meteo
// ------------------------------------------------------------------
// Dati mostrati a rotazione (ogni 4 s) nella riga delle condizioni
enum Info { INFO_COND, INFO_FEELS, INFO_WIND, INFO_PRES, INFO_SUNRISE, INFO_SUNSET, INFO_COUNT };

bool infoAvailable(uint8_t i) {
  switch (i) {
    case INFO_PRES:    return wx.pres > 0;
    case INFO_SUNRISE: return wx.sunrise >= 0;
    case INFO_SUNSET:  return wx.sunset >= 0;
    default:           return true;
  }
}

void nextInfo() {
  for (uint8_t n = 0; n < INFO_COUNT; n++) {
    infoIdx = (infoIdx + 1) % INFO_COUNT;
    if (infoAvailable(infoIdx)) return;
  }
}

// minuti dalla mezzanotte -> "06:45" (24 h) oppure "6:45 AM" (12 h)
void fmtClock(char* out, size_t n, int mins) {
  int h = mins / 60, m = mins % 60;
  if (cfg.h12) {
    const char* ap = (h >= 12) ? "PM" : "AM";
    h %= 12;
    if (h == 0) h = 12;
    snprintf(out, n, "%d:%02d %s", h, m, ap);
  } else {
    snprintf(out, n, "%02d:%02d", h, m);
  }
}

// Riga con una sola scritta centrata (condizioni del tempo)
void drawCondLine(const char* text) {
  const uint16_t cbg = col(th->card);
  int f = (tft.textWidth(text, 4) <= CARD_W - 16) ? 4 : 2;
  tft.fillRect(CARD_X + 6, DESC_Y, CARD_W - 12, 26, cbg);
  tft.setTextDatum(TC_DATUM);
  tft.setTextColor(col(th->text), cbg);
  tft.drawString(text, CARD_X + CARD_W / 2, DESC_Y + ((f == 4) ? 0 : 5), f);
  tft.setTextDatum(TL_DATUM);
}

// Riga a due colori: etichetta (colore del tema) + valore, centrata.
// Con deg=true dopo il valore disegna "°C" (i font non hanno il simbolo del grado).
void drawLabelValue(const char* label, const char* value, bool deg) {
  const uint16_t cbg = col(th->card);
  int f = 4, gap = 8, r = 3;
  int wl = tft.textWidth(label, f), wv = tft.textWidth(value, f);
  int wc = deg ? tft.textWidth("C", f) : 0;
  int total = wl + gap + wv + (deg ? (2 + (2 * r + 1) + 2 + wc) : 0);
  if (total > CARD_W - 16) {  // non entra: uso il font piccolo
    f = 2; gap = 5; r = 2;
    wl = tft.textWidth(label, f);
    wv = tft.textWidth(value, f);
    wc = deg ? tft.textWidth("C", f) : 0;
    total = wl + gap + wv + (deg ? (2 + (2 * r + 1) + 2 + wc) : 0);
  }
  int y = DESC_Y + ((f == 4) ? 0 : 5);
  int x = CARD_X + CARD_W / 2 - total / 2;

  tft.fillRect(CARD_X + 6, DESC_Y, CARD_W - 12, 26, cbg);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(col(th->a1), cbg);
  tft.drawString(label, x, y, f);
  x += wl + gap;
  tft.setTextColor(col(th->text), cbg);
  tft.drawString(value, x, y, f);
  if (deg) {
    x += wv + 2;
    int ry = y + ((f == 4) ? 8 : 5);
    tft.drawCircle(x + r, ry, r, col(th->a1));
    tft.drawCircle(x + r, ry, r - 1, col(th->a1));
    x += (2 * r + 1) + 2;
    tft.setTextColor(col(th->a1), cbg);
    tft.drawString("C", x, y, f);
  }
}

// Disegna il dato corrente della riga a rotazione
void drawInfoLine() {
  if (!wx.valid) {
    drawCondLine("Carico meteo...");
    return;
  }
  if (!infoAvailable(infoIdx)) infoIdx = INFO_COND;
  char v[28];
  switch (infoIdx) {
    case INFO_FEELS:
      snprintf(v, sizeof(v), "%d", (int)lroundf(wx.feels));
      drawLabelValue("Percepita", v, true);
      break;
    case INFO_WIND:
      snprintf(v, sizeof(v), "%.2f km/h", wx.wind);
      for (char* p = v; *p; p++) {
        if (*p == '.') *p = ',';
      }
      drawLabelValue("Vento", v, false);
      break;
    case INFO_PRES:
      snprintf(v, sizeof(v), "%d hPa", (int)lroundf(wx.pres));
      drawLabelValue("Atm", v, false);
      break;
    case INFO_SUNRISE:
      fmtClock(v, sizeof(v), wx.sunrise);
      drawLabelValue("Alba", v, false);
      break;
    case INFO_SUNSET:
      fmtClock(v, sizeof(v), wx.sunset);
      drawLabelValue("Tramonto", v, false);
      break;
    default:
      drawCondLine(descFor(wx.code));
      break;
  }
}

void drawCard() {
  uint16_t cbg = col(th->card);
  tft.setTextDatum(TL_DATUM);

  // ripulisco l'interno superiore (titolo, icona, temperatura) senza toccare gli angoli arrotondati
  tft.fillRect(CARD_X + 12, CARD_Y + 3, CARD_W - 24, 11, cbg);
  tft.fillRect(CARD_X + 3, CARD_Y + 14, CARD_W - 6, DESC_Y - (CARD_Y + 14), cbg);

  // icona
  Icon ic = wx.valid ? iconFor(wx.code, wx.day) : IC_CLOUD;
  drawWeatherIcon(ic, 46, 160, cbg);

  // temperatura
  const int tempY = CARD_Y + 21;
  char tb[8];
  if (wx.valid) snprintf(tb, sizeof(tb), "%d", (int)lroundf(wx.temp));
  else strlcpy(tb, "--", sizeof(tb));
  int tw = tft.textWidth(tb, 6);
  int blockW = tw + 8 + 8 + 16;
  int x0 = 88 + (138 - blockW) / 2;
  tft.setTextColor(col(th->text), cbg);
  tft.drawString(tb, x0, tempY, 6);
  tft.drawCircle(x0 + tw + 9, tempY + 8, 5, col(th->a1));
  tft.drawCircle(x0 + tw + 9, tempY + 8, 4, col(th->a1));
  tft.setTextColor(col(th->a1), cbg);
  tft.drawString("C", x0 + tw + 18, tempY + 3, 4);

  // citta': disegnata per ultima e senza sfondo, cosi' resta sempre in primo piano
  // (trasparente sopra icona e grafica)
  char city[24];
  strlcpy(city, cfg.city, sizeof(city));
  for (char* p = city; *p; p++) *p = toupper((unsigned char)*p);
  tft.setTextColor(col(th->a2));
  tft.drawString(city, CARD_X + 14, CARD_Y + 3, 2);

  // riga delle condizioni / dati a rotazione
  drawInfoLine();
  lastInfoRot = millis();

  // riga min / max / umidita'
  tft.fillRect(CARD_X + 16, 218, CARD_W - 32, 16, cbg);
  if (wx.valid) {
    char s[16];
    tft.setTextDatum(TC_DATUM);
    tft.setTextColor(col(0x5CC8FF), cbg);
    snprintf(s, sizeof(s), "MIN %d", (int)lroundf(wx.tmin));
    tft.drawString(s, 44, 218, 2);
    tft.setTextColor(col(0xFF8A5C), cbg);
    snprintf(s, sizeof(s), "MAX %d", (int)lroundf(wx.tmax));
    tft.drawString(s, 120, 218, 2);
    tft.setTextColor(col(th->a1), cbg);
    snprintf(s, sizeof(s), "UM %d%%", wx.hum);
    tft.drawString(s, 196, 218, 2);
    uint16_t sep = col(mixRGB(th->card, th->dim, 120));
    tft.drawFastVLine(82, 220, 12, sep);
    tft.drawFastVLine(158, 220, 12, sep);
  }
  tft.setTextDatum(TL_DATUM);
}

void fullRedraw() {
  th = &THEMES[cfg.theme];
  tft.setRotation(cfg.flip ? 2 : 0);
  tft.invertDisplay(cfg.invert);
  tft.fillScreen(col(th->bg));
  tft.fillRoundRect(CARD_X, CARD_Y, CARD_W, CARD_H, 14, col(th->card));
  tft.drawRoundRect(CARD_X, CARD_Y, CARD_W, CARD_H, 14, col(mixRGB(th->card, th->a1, 100)));
  lastDigitsKey = -1;
  lastHdrKey = -1000;
  lastSec = -1;
  lastNow = 0;
  wxDirty = true;
}

// ------------------------------------------------------------------
//  Schermata previsioni (prossimi 3 giorni)
// ------------------------------------------------------------------
// numero + simbolo del grado (i font non hanno il carattere)
void drawTempDeg(int x, int y, int value, uint16_t color, uint16_t cbg) {
  char b[8];
  snprintf(b, sizeof(b), "%d", value);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(color, cbg);
  tft.drawString(b, x, y, 4);
  int w = tft.textWidth(b, 4);
  tft.drawCircle(x + w + 5, y + 6, 3, color);
  tft.drawCircle(x + w + 5, y + 6, 2, color);
}

void drawForecast() {
  const uint16_t bg = col(th->bg), cbg = col(th->card);
  const uint16_t hot = col(0xFF8A5C), cold = col(0x5CC8FF);
  tft.fillScreen(bg);
  drawBand();
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(col(th->text));
  tft.drawString("PREVISIONI", 10, 1, 4);

  char city[24];
  strlcpy(city, cfg.city, sizeof(city));
  for (char* p = city; *p; p++) *p = toupper((unsigned char)*p);
  while (strlen(city) > 1 && tft.textWidth(city, 2) > 84) city[strlen(city) - 1] = 0;
  tft.setTextDatum(TR_DATUM);
  tft.setTextColor(col(th->a1));
  tft.drawString(city, 232, 7, 2);
  tft.setTextDatum(TL_DATUM);

  // mezzogiorno di oggi: cosi' +24 h non sbaglia giorno nei cambi ora legale
  time_t now = time(nullptr);
  struct tm base;
  localtime_r(&now, &base);
  base.tm_hour = 12;
  base.tm_min = 0;
  base.tm_sec = 0;
  base.tm_isdst = -1;
  time_t noon = mktime(&base);

  for (int i = 0; i < 3; i++) {
    const int y = 33 + i * 68;
    tft.fillRoundRect(6, y, 228, 64, 12, cbg);
    tft.drawRoundRect(6, y, 228, 64, 12, col(mixRGB(th->card, th->a1, 100)));

    time_t t = noon + (time_t)(i + 1) * 86400;
    struct tm ti;
    localtime_r(&t, &ti);
    char ds[16];
    snprintf(ds, sizeof(ds), "%d %s", ti.tm_mday, MONTHS[ti.tm_mon]);
    tft.setTextColor(col(th->a1), cbg);
    tft.drawString(DAYS[ti.tm_wday], 18, y + 8, 4);
    tft.setTextColor(col(mixRGB(th->dim, th->text, 110)), cbg);
    tft.drawString(ds, 18, y + 38, 2);

    drawWeatherIcon(iconFor(wx.fcCode[i], true), 112, y + 32, cbg);

    // massima (freccia su) e minima (freccia giu')
    tft.fillTriangle(146, y + 12, 142, y + 20, 150, y + 20, hot);
    drawTempDeg(156, y + 6, (int)lroundf(wx.fcMax[i]), hot, cbg);
    tft.fillTriangle(142, y + 42, 150, y + 42, 146, y + 50, cold);
    drawTempDeg(156, y + 34, (int)lroundf(wx.fcMin[i]), cold, cbg);
  }
}

// ------------------------------------------------------------------
//  Helper grafici per le schermate lunare e citazione
// ------------------------------------------------------------------
static uint16_t cf565(uint8_t r, uint8_t g, uint8_t b) {
  return cfg.swapRB ? tft.color565(b, g, r) : tft.color565(r, g, b);
}
static void rowPush(int x, int y, int w, const uint16_t* px) {
  tft.pushImage(x, y, w, 1, (uint16_t*)px);
}
// 16 livelli di trasparenza tra sfondo e testo (per i font anti-aliased)
static void makeLut(uint16_t* lut, uint32_t bg, uint32_t fg) {
  for (int a = 0; a < 16; a++) lut[a] = col(mixRGB(bg, fg, (uint8_t)(a * 17)));
}
bool timeOk() { return time(nullptr) > 1700000000; }

static const char* const MONTHS_L[] = {"gen", "feb", "mar", "apr", "mag", "giu", "lug", "ago", "set", "ott", "nov", "dic"};

// intestazione comune: titolo a sinistra, data a destra
void drawTitleBar(const char* title, const struct tm& ti) {
  drawBand();
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(col(th->text));
  tft.drawString(title, 10, 1, 4);
  char ds[12];
  snprintf(ds, sizeof(ds), "%d %s", ti.tm_mday, MONTHS[ti.tm_mon]);
  tft.setTextDatum(TR_DATUM);
  tft.setTextColor(col(th->a1));
  tft.drawString(ds, 232, 7, 2);
  tft.setTextDatum(TL_DATUM);
}

// numero di giorno locale (giorni dal 1970), per scegliere la citazione e contare i giorni
long dayNumber(const struct tm& t) {
  long y = t.tm_year + 1900, m = t.tm_mon + 1, d = t.tm_mday;
  y -= m <= 2;
  long era = (y >= 0 ? y : y - 399) / 400;
  long yoe = y - era * 400;
  long doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + doe - 719468;
}

// ------------------------------------------------------------------
//  Schermata fasi lunari
// ------------------------------------------------------------------
void drawMoon() {
  const uint16_t bg = col(th->bg), cbg = col(th->card);
  tft.fillScreen(bg);
  time_t now = time(nullptr);
  struct tm ti;
  localtime_r(&now, &ti);
  drawTitleBar("FASE LUNARE", ti);

  const moon::Phase p = moon::phaseAt((double)now);
  moon::drawDisc(120, 72, 40, 60, (float)p.elong, th->bg, th->a1, cf565, rowPush, HEADER_H + 1);

  // nome della fase, centrato
  uint16_t lut[16];
  makeLut(lut, th->bg, th->text);
  const char* nm = moon::phaseName(p);
  int nw = qr::width8(QF_T1, nm, (int)strlen(nm)) / 8;
  qr::drawLine(QF_T1, nm, (int)strlen(nm), 0, 120, 240, lut, rowPush, 0, (240 - nw) / 2);

  // due riquadri: illuminazione e prossima luna piena
  const int cy = 160, ch = 72, cw = 111;
  const int cx[2] = {6, 123};
  uint16_t lutV[16];
  makeLut(lutV, th->card, th->text);
  for (int i = 0; i < 2; i++) {
    tft.fillRoundRect(cx[i], cy, cw, ch, 12, cbg);
    tft.drawRoundRect(cx[i], cy, cw, ch, 12, col(mixRGB(th->card, th->a1, 100)));
  }
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(col(th->a1), cbg);
  tft.drawString("ILLUMINATA", cx[0] + 10, cy + 6, 2);
  tft.drawString("LUNA PIENA", cx[1] + 10, cy + 6, 2);

  char buf[24];
  snprintf(buf, sizeof(buf), "%d%%", (int)lroundf((float)p.lit * 100.0f));
  qr::drawLine(QF_T0, buf, (int)strlen(buf), cx[0] + 8, cy + 20, 95, lutV, rowPush);
  tft.setTextColor(col(mixRGB(th->dim, th->text, 110)), cbg);
  tft.drawString(p.elong < 180.0 ? "in crescita" : "in calo", cx[0] + 10, cy + 54, 2);

  const double nf = moon::nextFullMoon((double)now);
  time_t nt = (time_t)nf;
  struct tm tf;
  localtime_r(&nt, &tf);
  const long days = dayNumber(tf) - dayNumber(ti);
  if (days <= 0) strlcpy(buf, "oggi", sizeof(buf));
  else snprintf(buf, sizeof(buf), "%d %s", tf.tm_mday, MONTHS_L[tf.tm_mon]);
  qr::drawLine(QF_T0, buf, (int)strlen(buf), cx[1] + 8, cy + 20, 95, lutV, rowPush);
  if (days <= 0) snprintf(buf, sizeof(buf), "alle %02d:%02d", tf.tm_hour, tf.tm_min);
  else if (days == 1) strlcpy(buf, "domani", sizeof(buf));
  else snprintf(buf, sizeof(buf), "tra %ld giorni", days);
  tft.drawString(buf, cx[1] + 10, cy + 54, 2);
}

// ------------------------------------------------------------------
//  Schermata citazione del giorno
// ------------------------------------------------------------------
void drawQuote() {
  const uint16_t bg = col(th->bg), cbg = col(th->card);
  tft.fillScreen(bg);
  time_t now = time(nullptr);
  struct tm ti;
  localtime_r(&now, &ti);
  drawTitleBar("CITAZIONE", ti);
  tft.fillRoundRect(6, 33, 228, 201, 14, cbg);
  tft.drawRoundRect(6, 33, 228, 201, 14, col(mixRGB(th->card, th->a1, 100)));

  uint16_t lutM[16], lutT[16], lutA[16];
  makeLut(lutM, th->card, th->a2);
  makeLut(lutT, th->card, th->text);
  makeLut(lutA, th->card, th->a1);

  // virgolette decorative
  QGlyph g;
  int mw = qr::glyph(QF_MARK, 0x201C, g) ? g.w : 30;
  qr::drawLine(QF_MARK, "\xE2\x80\x9C", 3, 16, 38, 208, lutM, rowPush, 0, (208 - mw) / 2);

  // la citazione cambia a mezzanotte
  const int idx = (int)(dayNumber(ti) % QUOTE_COUNT);
  char qt[200], qa[48];
  strncpy_P(qt, QUOTES[idx].text, sizeof(qt) - 1);
  qt[sizeof(qt) - 1] = 0;
  strncpy_P(qa, QUOTES[idx].author, sizeof(qa) - 1);
  qa[sizeof(qa) - 1] = 0;
  tft.drawFastHLine(100, 203, 40, col(mixRGB(th->card, th->a2, 170)));
  qr::drawQuoteBody(qt, qa, 16, 70, 208, 158, lutT, lutA, rowPush);
}

// ------------------------------------------------------------------
//  Foto (salvate in LittleFS come JPEG 240x240)
// ------------------------------------------------------------------
uint8_t photoSlots[MAX_PHOTOS];  // numeri dei file presenti, in ordine
uint8_t photoCount = 0;
uint8_t photoCursor = 0;         // prossima foto da mostrare
uint32_t photoRev = 1;           // cambia a ogni aggiunta/eliminazione (per la cache del browser)

void photoPath(char* out, size_t n, int slot) { snprintf(out, n, "/ph%d.jpg", slot); }

void scanPhotos() {
  photoCount = 0;
  char p[16];
  for (int i = 0; i < MAX_PHOTOS; i++) {
    photoPath(p, sizeof(p), i);
    if (LittleFS.exists(p)) photoSlots[photoCount++] = (uint8_t)i;
  }
  if (photoCursor >= photoCount) photoCursor = 0;
}

int jpgRead(void* ctx, uint8_t* buf, int len) { return (int)((File*)ctx)->read(buf, (size_t)len); }
void jpgOut(void*, int x, int y, int w, int h, uint16_t* px) {
  tft.pushImage(x, y, w, h, px);
  yield();
}

// Mostra una foto a schermo intero. Ritorna false se il file non e' leggibile.
bool drawPhoto(uint8_t slot) {
  char p[16];
  photoPath(p, sizeof(p), slot);
  File f = LittleFS.open(p, "r");
  if (!f) return false;
  JpegDecoder* dec = new (std::nothrow) JpegDecoder(jpgRead, &f);
  if (!dec) {
    f.close();
    return false;
  }
  bool ok = false;
  if (dec->begin() == JpegDecoder::OK) {
    if (dec->width() < 240 || dec->height() < 240) tft.fillScreen(0);  // foto piu' piccola: sfondo nero
    ok = (dec->decode(jpgOut, nullptr, cfg.swapRB) == JpegDecoder::OK);
  }
  delete dec;
  f.close();
  return ok;
}

// ------------------------------------------------------------------
//  Sequenza delle schermate:
//  principale (24 s) -> previsioni -> luna -> citazione -> foto -> principale -> ...
//  (le schermate non attive vengono saltate; una sola foto per giro, in ordine di caricamento)
// ------------------------------------------------------------------
enum ScreenId { SCR_MAIN, SCR_FORECAST, SCR_MOON, SCR_QUOTE, SCR_PHOTO };
ScreenId screen = SCR_MAIN;
uint32_t screenStart = 0;
static const uint32_t MAIN_MS = INFO_COUNT * INFO_ROTATE_MS;  // 6 dati x 4 s = 24 s

bool photosReady() { return cfg.phOn && photoCount > 0; }

void enterMain() {
  screen = SCR_MAIN;
  infoIdx = INFO_COND;  // la riga delle condizioni riparte dalle condizioni del tempo
  fullRedraw();
  screenStart = millis();
}

void enterSimple(ScreenId id) {
  screen = id;
  if (id == SCR_FORECAST) drawForecast();
  else if (id == SCR_MOON) drawMoon();
  else drawQuote();
  screenStart = millis();
}

// mostra la prossima foto utile (salta quelle illeggibili)
bool enterPhoto() {
  for (uint8_t n = 0; n < photoCount; n++) {
    uint8_t slot = photoSlots[photoCursor % photoCount];
    photoCursor = (photoCursor + 1) % photoCount;
    if (drawPhoto(slot)) {
      screen = SCR_PHOTO;
      screenStart = millis();
      return true;
    }
  }
  return false;
}

// la schermata e' attiva e ha qualcosa da mostrare?
bool screenReady(ScreenId id) {
  switch (id) {
    case SCR_FORECAST: return cfg.fcOn && wx.fcValid;
    case SCR_MOON: return cfg.moOn && timeOk();
    case SCR_QUOTE: return cfg.qtOn && timeOk();
    case SCR_PHOTO: return photosReady();
    default: return false;
  }
}

bool enterScreen(ScreenId id) {
  if (id == SCR_PHOTO) return enterPhoto();
  enterSimple(id);
  return true;
}

uint32_t screenMs(ScreenId id) {
  switch (id) {
    case SCR_FORECAST: return (uint32_t)cfg.fcSec * 1000UL;
    case SCR_MOON: return (uint32_t)cfg.moSec * 1000UL;
    case SCR_QUOTE: return (uint32_t)cfg.qtSec * 1000UL;
    case SCR_PHOTO: return (uint32_t)cfg.phSec * 1000UL;
    default: return MAIN_MS;
  }
}

// Fine di una schermata: passa alla prossima attiva nell'ordine
// principale -> previsioni -> luna -> citazione -> foto -> principale
void advance(ScreenId from) {
  for (int n = (int)from + 1; n <= (int)SCR_PHOTO; n++) {
    if (screenReady((ScreenId)n) && enterScreen((ScreenId)n)) return;
  }
  if (from == SCR_MAIN) screenStart = millis();  // niente da alternare: resto qui
  else enterMain();
}

void drawSplash(const char* msg) {
  static bool first = true;
  uint16_t bg = col(th->bg);
  if (first) {
    first = false;
    tft.fillScreen(bg);
    tft.setTextSize(2);
    tft.setTextDatum(TL_DATUM);
    int w1 = tft.textWidth("SMALL", 4);
    int w2 = tft.textWidth("TV", 4);
    int gap = 12;
    int x0 = (240 - (w1 + gap + w2)) / 2;
    tft.setTextColor(col(th->a1));
    tft.drawString("SMALL", x0, 62, 4);
    tft.setTextColor(col(th->a2));
    tft.drawString("TV", x0 + w1 + gap, 62, 4);
    tft.setTextSize(1);
    for (int i = 0; i < 40; i++) {
      tft.fillRect(40 + i * 4, 128, 4, 3, col(mixRGB(th->a1, th->a2, (uint8_t)(i * 255 / 39))));
    }
    tft.setTextColor(col(th->dim));
    tft.setTextDatum(TC_DATUM);
    tft.drawString("U L T R A", 120, 142, 2);
  }
  tft.setTextDatum(TC_DATUM);
  tft.setTextPadding(220);
  tft.setTextColor(col(th->text), bg);
  tft.drawString(msg, 120, 200, 2);
  tft.setTextPadding(0);
  tft.setTextDatum(TL_DATUM);
}

void drawApScreen() {
  uint16_t bg = col(th->bg);
  tft.fillScreen(bg);
  drawBand();
  tft.setTextDatum(TC_DATUM);
  tft.setTextColor(col(th->text));
  tft.drawString("CONFIGURA WI-FI", 120, 1, 4);

  tft.setTextColor(col(th->dim));
  tft.drawString("1. Collegati alla rete Wi-Fi", 120, 42, 2);
  tft.setTextColor(col(th->a2));
  tft.drawString(AP_SSID, 120, 62, 4);
  tft.setTextColor(col(th->dim));
  tft.drawString("password", 120, 96, 2);
  tft.setTextColor(col(th->text));
  tft.drawString(AP_PASS, 120, 112, 4);

  tft.setTextColor(col(th->dim));
  tft.drawString("2. Si apre la pagina di setup", 120, 154, 2);
  tft.drawString("(altrimenti vai su)", 120, 174, 2);
  tft.setTextColor(col(th->a1));
  tft.drawString("192.168.4.1", 120, 196, 4);
  tft.setTextDatum(TL_DATUM);
}

void drawIpScreen(const char* ip) {
  uint16_t bg = col(th->bg);
  tft.fillScreen(bg);
  drawBand();
  tft.setTextDatum(TC_DATUM);
  tft.setTextColor(col(th->text));
  tft.drawString("CONNESSO", 120, 1, 4);
  tft.setTextColor(col(th->dim));
  tft.drawString("Personalizza da browser:", 120, 70, 2);
  tft.setTextColor(col(th->a1));
  tft.drawString("smalltv.local", 120, 96, 4);
  tft.setTextColor(col(th->dim));
  tft.drawString("oppure", 120, 136, 2);
  tft.setTextColor(col(th->a2));
  tft.drawString(ip, 120, 156, 4);
  tft.setTextDatum(TL_DATUM);
}

// ------------------------------------------------------------------
//  Orologio
// ------------------------------------------------------------------
void updateClock() {
  time_t now = time(nullptr);
  if (now == lastNow) return;
  lastNow = now;
  bool synced = now > 1700000000;
  struct tm ti;
  localtime_r(&now, &ti);

  int lvl = wifiLevel();
  int hdrKey = synced ? (((ti.tm_year * 12 + ti.tm_mon) * 32 + ti.tm_mday) * 8 + lvl) : (-2 - lvl);
  if (hdrKey != lastHdrKey) {
    drawHeader(&ti, synced);
    lastHdrKey = hdrKey;
  }

  int digitsKey = synced ? (ti.tm_hour * 60 + ti.tm_min) : -5;
  if (digitsKey != lastDigitsKey) {
    drawTimeDigits(synced ? ti.tm_hour : 0, synced ? ti.tm_min : 0, synced);
    lastDigitsKey = digitsKey;
  }

  drawColon((ti.tm_sec & 1) == 0);
  int sec = synced ? ti.tm_sec : 0;
  if (sec != lastSec) {
    drawSecBar(sec);
    lastSec = sec;
  }
}

// ------------------------------------------------------------------
//  Meteo (Open-Meteo)
// ------------------------------------------------------------------
// "2026-10-31T06:45" -> minuti dalla mezzanotte (-1 se non valido)
int parseHM(const char* iso) {
  const char* t = iso ? strchr(iso, 'T') : nullptr;
  int h, m;
  if (t && sscanf(t + 1, "%d:%d", &h, &m) == 2 && h >= 0 && h < 24 && m >= 0 && m < 60) return h * 60 + m;
  return -1;
}

bool fetchWeather() {
  if (WiFi.status() != WL_CONNECTED) return false;

  BearSSL::WiFiClientSecure client;
  client.setInsecure();
  bool mfln = client.probeMaxFragmentLength("api.open-meteo.com", 443, 1024);
  if (mfln) {
    client.setBufferSizes(1024, 1024);
  } else if (ESP.getFreeHeap() < 26000) {
    Serial.println("Meteo: RAM insufficiente per TLS");
    return false;
  }

  char url[480];
  snprintf(url, sizeof(url),
           "https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f"
           "&current=temperature_2m,relative_humidity_2m,apparent_temperature,weather_code,is_day,"
           "wind_speed_10m,pressure_msl"
           "&daily=weather_code,temperature_2m_max,temperature_2m_min,sunrise,sunset"
           "&timezone=Europe%%2FRome&forecast_days=4",
           cfg.lat, cfg.lon);

  HTTPClient http;
  http.setTimeout(8000);
  http.useHTTP10(true);
  if (!http.begin(client, url)) return false;
  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    Serial.printf("Meteo: HTTP %d\n", code);
    http.end();
    return false;
  }

  DynamicJsonDocument filter(768);
  filter["current"]["temperature_2m"] = true;
  filter["current"]["relative_humidity_2m"] = true;
  filter["current"]["apparent_temperature"] = true;
  filter["current"]["weather_code"] = true;
  filter["current"]["is_day"] = true;
  filter["current"]["wind_speed_10m"] = true;
  filter["current"]["pressure_msl"] = true;
  filter["daily"]["weather_code"][0] = true;
  filter["daily"]["temperature_2m_max"][0] = true;
  filter["daily"]["temperature_2m_min"][0] = true;
  filter["daily"]["sunrise"][0] = true;
  filter["daily"]["sunset"][0] = true;

  DynamicJsonDocument doc(1536);
  DeserializationError err = deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
  http.end();
  if (err) {
    Serial.printf("Meteo: JSON %s\n", err.c_str());
    return false;
  }

  JsonObject cur = doc["current"];
  if (cur.isNull()) return false;
  wx.temp = cur["temperature_2m"] | 0.0f;
  wx.hum = cur["relative_humidity_2m"] | 0;
  wx.code = cur["weather_code"] | 0;
  wx.day = (cur["is_day"] | 1) != 0;
  wx.feels = cur["apparent_temperature"] | wx.temp;
  wx.wind = cur["wind_speed_10m"] | 0.0f;
  wx.pres = cur["pressure_msl"] | 0.0f;
  wx.tmax = doc["daily"]["temperature_2m_max"][0] | wx.temp;
  wx.tmin = doc["daily"]["temperature_2m_min"][0] | wx.temp;
  wx.sunrise = parseHM(doc["daily"]["sunrise"][0] | "");
  wx.sunset = parseHM(doc["daily"]["sunset"][0] | "");
  // previsioni dei 3 giorni successivi (indici 1..3 degli array giornalieri)
  bool fcOk = true;
  for (int i = 0; i < 3; i++) {
    int fc = doc["daily"]["weather_code"][i + 1] | -1;
    float fmx = doc["daily"]["temperature_2m_max"][i + 1] | -999.0f;
    float fmn = doc["daily"]["temperature_2m_min"][i + 1] | -999.0f;
    if (fc < 0 || fmx < -900.0f || fmn < -900.0f) fcOk = false;
    wx.fcCode[i] = fc;
    wx.fcMax[i] = fmx;
    wx.fcMin[i] = fmn;
  }
  wx.fcValid = fcOk;
  wx.fetchedAt = millis();
  wx.valid = true;
  wxDirty = true;
  Serial.printf("Meteo OK: %.1f C codice %d\n", wx.temp, wx.code);
  return true;
}

// ------------------------------------------------------------------
//  Web server
// ------------------------------------------------------------------
void noCache() { server.sendHeader("Cache-Control", "no-store"); }

void sendJsonDoc(const JsonDocument& d) {
  String out;
  serializeJson(d, out);
  noCache();
  server.send(200, "application/json", out);
}

// "HH:MM" <-> minuti dalla mezzanotte
void minToStr(char* out, size_t n, uint16_t m) {
  snprintf(out, n, "%02d:%02d", m / 60, m % 60);
}

bool strToMin(const char* s, uint16_t& out) {
  int h, m;
  if (s && sscanf(s, "%d:%d", &h, &m) == 2 && h >= 0 && h < 24 && m >= 0 && m < 60) {
    out = (uint16_t)(h * 60 + m);
    return true;
  }
  return false;
}

void handleConfigGet() {
  DynamicJsonDocument d(768);
  d["city"] = cfg.city;
  d["lat"] = cfg.lat;
  d["lon"] = cfg.lon;
  d["theme"] = cfg.theme;
  d["bright"] = cfg.bright;
  d["wx"] = cfg.wxMinutes;
  d["flip"] = cfg.flip;
  d["invert"] = cfg.invert;
  d["swap"] = cfg.swapRB;
  d["ssid"] = cfg.ssid;
  d["h12"] = cfg.h12;
  d["night"] = cfg.nightOn;
  char t[8];
  minToStr(t, sizeof(t), cfg.nStart);
  d["ns"] = String(t);
  minToStr(t, sizeof(t), cfg.nEnd);
  d["ne"] = String(t);
  d["nb"] = cfg.nBright;
  d["fc"] = cfg.fcOn;
  d["fcs"] = cfg.fcSec;
  d["ph"] = cfg.phOn;
  d["phs"] = cfg.phSec;
  d["mo"] = cfg.moOn;
  d["mos"] = cfg.moSec;
  d["qt"] = cfg.qtOn;
  d["qts"] = cfg.qtSec;
  sendJsonDoc(d);
}

void handleConfigPost() {
  DynamicJsonDocument d(1024);
  if (deserializeJson(d, server.arg("plain"))) {
    server.send(400, "application/json", "{\"ok\":false}");
    return;
  }
  uint8_t oldTheme = cfg.theme;
  bool oldFlip = cfg.flip, oldInv = cfg.invert, oldSwap = cfg.swapRB, oldH12 = cfg.h12;
  float oldLat = cfg.lat, oldLon = cfg.lon;

  if (d.containsKey("city")) {
    asciiCopy(cfg.city, sizeof(cfg.city), d["city"] | "ROMA");
    if (cfg.city[0] == 0) strlcpy(cfg.city, "ROMA", sizeof(cfg.city));
  }
  if (d.containsKey("lat")) cfg.lat = constrain(d["lat"].as<float>(), -90.0f, 90.0f);
  if (d.containsKey("lon")) cfg.lon = constrain(d["lon"].as<float>(), -180.0f, 180.0f);
  if (d.containsKey("theme")) cfg.theme = constrain(d["theme"].as<int>(), 0, (int)THEME_COUNT - 1);
  if (d.containsKey("bright")) cfg.bright = constrain(d["bright"].as<int>(), 5, 100);
  if (d.containsKey("wx")) cfg.wxMinutes = constrain(d["wx"].as<int>(), 5, 120);
  if (d.containsKey("flip")) cfg.flip = d["flip"].as<bool>();
  if (d.containsKey("invert")) cfg.invert = d["invert"].as<bool>();
  if (d.containsKey("swap")) cfg.swapRB = d["swap"].as<bool>();
  if (d.containsKey("h12")) cfg.h12 = d["h12"].as<bool>();
  if (d.containsKey("night")) cfg.nightOn = d["night"].as<bool>();
  uint16_t mm;
  if (d.containsKey("ns") && strToMin(d["ns"] | "", mm)) cfg.nStart = mm;
  if (d.containsKey("ne") && strToMin(d["ne"] | "", mm)) cfg.nEnd = mm;
  if (d.containsKey("nb")) cfg.nBright = constrain(d["nb"].as<int>(), 0, 100);
  if (d.containsKey("fc")) cfg.fcOn = d["fc"].as<bool>();
  if (d.containsKey("fcs")) cfg.fcSec = constrain(d["fcs"].as<int>(), 2, 60);
  if (d.containsKey("ph")) cfg.phOn = d["ph"].as<bool>();
  if (d.containsKey("phs")) cfg.phSec = constrain(d["phs"].as<int>(), 2, 60);
  if (d.containsKey("mo")) cfg.moOn = d["mo"].as<bool>();
  if (d.containsKey("mos")) cfg.moSec = constrain(d["mos"].as<int>(), 2, 60);
  if (d.containsKey("qt")) cfg.qtOn = d["qt"].as<bool>();
  if (d.containsKey("qts")) cfg.qtSec = constrain(d["qts"].as<int>(), 2, 60);

  saveConfig();
  applyBrightness(true);

  if (cfg.theme != oldTheme || cfg.flip != oldFlip || cfg.invert != oldInv || cfg.swapRB != oldSwap ||
      cfg.h12 != oldH12) needRedraw = true;
  else wxDirty = true;
  if (fabsf(cfg.lat - oldLat) > 0.0001f || fabsf(cfg.lon - oldLon) > 0.0001f) {
    wx.valid = false;
    nextWx = millis();
  }
  noCache();
  server.send(200, "application/json", "{\"ok\":true}");
}

void handleStatus() {
  DynamicJsonDocument d(768);
  time_t now = time(nullptr);
  bool synced = now > 1700000000;
  struct tm ti;
  localtime_r(&now, &ti);
  char tb[12], db[12];
  strftime(tb, sizeof(tb), "%H:%M:%S", &ti);
  strftime(db, sizeof(db), "%Y-%m-%d", &ti);
  d["fw"] = FW_VERSION;
  d["ip"] = apMode ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
  d["ssid"] = WiFi.SSID();
  d["rssi"] = WiFi.RSSI();
  d["heap"] = ESP.getFreeHeap();
  d["up"] = millis() / 1000;
  d["synced"] = synced;
  d["time"] = tb;
  d["date"] = db;
  d["nm"] = nightActive();
  JsonObject w = d.createNestedObject("wx");
  w["ok"] = wx.valid;
  if (wx.valid) {
    w["t"] = wx.temp;
    w["hum"] = wx.hum;
    w["code"] = wx.code;
    w["min"] = wx.tmin;
    w["max"] = wx.tmax;
  }
  sendJsonDoc(d);
}

// ---- foto: elenco, anteprima, caricamento, eliminazione ----
static File upFile;
static int upSlot = -1;
static size_t upBytes = 0;
static bool upOk = false;
static const char* upErr = "errore sconosciuto";

void upAbort(const char* why) {
  if (upFile) upFile.close();
  if (upSlot >= 0) {
    char p[16];
    photoPath(p, sizeof(p), upSlot);
    LittleFS.remove(p);
  }
  upSlot = -1;
  upOk = false;
  upErr = why;
}

// Riceve il file a blocchi (multipart) e lo scrive direttamente in LittleFS
void handlePhotoUpload() {
  HTTPUpload& u = server.upload();
  if (u.status == UPLOAD_FILE_START) {
    upOk = false;
    upErr = "errore sconosciuto";
    upBytes = 0;
    upSlot = -1;
    if (upFile) upFile.close();
    char p[16];
    for (int i = 0; i < MAX_PHOTOS; i++) {
      photoPath(p, sizeof(p), i);
      if (!LittleFS.exists(p)) {
        upSlot = i;
        break;
      }
    }
    if (upSlot < 0) {
      upErr = "limite di foto raggiunto";
      return;
    }
    FSInfo fi;
    LittleFS.info(fi);
    if (fi.totalBytes - fi.usedBytes < 64UL * 1024UL) {
      upSlot = -1;
      upErr = "memoria del dispositivo piena";
      return;
    }
    photoPath(p, sizeof(p), upSlot);
    upFile = LittleFS.open(p, "w");
    if (!upFile) {
      upSlot = -1;
      upErr = "scrittura non riuscita";
    }
  } else if (u.status == UPLOAD_FILE_WRITE) {
    if (!upFile) return;
    if (upBytes + u.currentSize > MAX_PHOTO_BYTES) {
      upAbort("foto troppo grande");
      return;
    }
    if (upFile.write(u.buf, u.currentSize) != u.currentSize) {
      upAbort("memoria del dispositivo piena");
      return;
    }
    upBytes += u.currentSize;
  } else if (u.status == UPLOAD_FILE_END) {
    if (!upFile) return;
    upFile.close();
    // controllo: deve essere un JPEG decodificabile
    char p[16];
    photoPath(p, sizeof(p), upSlot);
    bool valid = false;
    File f = LittleFS.open(p, "r");
    if (f) {
      JpegDecoder* d = new (std::nothrow) JpegDecoder(jpgRead, &f);
      if (d) {
        valid = (d->begin() == JpegDecoder::OK);
        delete d;
      }
      f.close();
    }
    if (!valid) {
      upAbort("formato non valido");
      return;
    }
    upOk = true;
  } else if (u.status == UPLOAD_FILE_ABORTED) {
    upAbort("invio interrotto");
  }
}

void handlePhotoDone() {
  noCache();
  if (upOk) {
    scanPhotos();
    photoRev++;
    char b[40];
    snprintf(b, sizeof(b), "{\"ok\":true,\"i\":%d}", upSlot);
    server.send(200, "application/json", b);
  } else {
    String s = String("{\"ok\":false,\"err\":\"") + upErr + "\"}";
    server.send(400, "application/json", s);
  }
  upOk = false;
  upSlot = -1;
}

void handlePhotoList() {
  FSInfo fi;
  LittleFS.info(fi);
  String s;
  s.reserve(96 + photoCount * 28);
  s += "{\"max\":";
  s += MAX_PHOTOS;
  s += ",\"rev\":";
  s += photoRev;
  s += ",\"free\":";
  s += (uint32_t)(fi.totalBytes - fi.usedBytes);
  s += ",\"photos\":[";
  char p[16];
  for (uint8_t k = 0; k < photoCount; k++) {
    photoPath(p, sizeof(p), photoSlots[k]);
    size_t sz = 0;
    File f = LittleFS.open(p, "r");
    if (f) {
      sz = f.size();
      f.close();
    }
    if (k) s += ',';
    s += "{\"i\":";
    s += (int)photoSlots[k];
    s += ",\"s\":";
    s += (uint32_t)sz;
    s += '}';
  }
  s += "]}";
  noCache();
  server.send(200, "application/json", s);
}

void handlePhotoGet() {
  int i = server.arg("i").toInt();
  char p[16];
  photoPath(p, sizeof(p), i);
  if (i < 0 || i >= MAX_PHOTOS || !LittleFS.exists(p)) {
    server.send(404, "text/plain", "Non trovata");
    return;
  }
  File f = LittleFS.open(p, "r");
  server.sendHeader("Cache-Control", "max-age=31536000");
  server.streamFile(f, "image/jpeg");
  f.close();
}

void handlePhotoDelete() {
  int i = server.arg("i").toInt();
  char p[16];
  photoPath(p, sizeof(p), i);
  noCache();
  if (i < 0 || i >= MAX_PHOTOS || !LittleFS.exists(p)) {
    server.send(404, "application/json", "{\"ok\":false}");
    return;
  }
  LittleFS.remove(p);
  scanPhotos();
  photoRev++;
  server.send(200, "application/json", "{\"ok\":true}");
}

void handleScan() {
  int n = WiFi.scanComplete();
  if (n == WIFI_SCAN_FAILED) {
    WiFi.scanNetworks(true, true);
    noCache();
    server.send(200, "application/json", "{\"scanning\":true}");
    return;
  }
  if (n == WIFI_SCAN_RUNNING) {
    noCache();
    server.send(200, "application/json", "{\"scanning\":true}");
    return;
  }
  DynamicJsonDocument d(2048);
  JsonArray arr = d.createNestedArray("networks");
  int count = 0;
  for (int i = 0; i < n && count < 14; i++) {
    String s = WiFi.SSID(i);
    if (s.length() == 0) continue;
    bool dup = false;
    for (JsonObject o : arr) {
      if (s == o["ssid"].as<const char*>()) { dup = true; break; }
    }
    if (dup) continue;
    JsonObject o = arr.createNestedObject();
    o["ssid"] = s;
    o["rssi"] = WiFi.RSSI(i);
    o["enc"] = WiFi.encryptionType(i) != ENC_TYPE_NONE;
    count++;
  }
  WiFi.scanDelete();
  sendJsonDoc(d);
}

void scheduleReboot(uint32_t ms) {
  pendingReboot = true;
  rebootAt = millis() + ms;
}

void handleWifiSave() {
  StaticJsonDocument<256> d;
  if (deserializeJson(d, server.arg("plain"))) {
    server.send(400, "application/json", "{\"ok\":false}");
    return;
  }
  const char* ssid = d["ssid"] | "";
  if (strlen(ssid) == 0 || strlen(ssid) > 32) {
    server.send(400, "application/json", "{\"ok\":false}");
    return;
  }
  strlcpy(cfg.ssid, ssid, sizeof(cfg.ssid));
  strlcpy(cfg.pass, d["pass"] | "", sizeof(cfg.pass));
  saveConfig();
  server.send(200, "application/json", "{\"ok\":true}");
  scheduleReboot(1500);
}

void handleNotFound() {
  if (apMode) {  // captive portal: tutto cio' che non conosciamo porta alla pagina di setup
    server.sendHeader("Location", "http://192.168.4.1/", true);
    server.send(302, "text/plain", "");
  } else {
    server.send(404, "text/plain", "Non trovato");
  }
}

void setupWebRoutes() {
  server.on("/", HTTP_GET, []() {
    noCache();
    server.send_P(200, "text/html", apMode ? WIFI_HTML : INDEX_HTML);
  });
  server.on("/wifi", HTTP_GET, []() {
    noCache();
    server.send_P(200, "text/html", WIFI_HTML);
  });
  server.on("/api/config", HTTP_GET, handleConfigGet);
  server.on("/api/config", HTTP_POST, handleConfigPost);
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/scan", HTTP_GET, handleScan);
  server.on("/api/wifi", HTTP_POST, handleWifiSave);
  server.on("/api/refresh", HTTP_POST, []() {
    nextWx = millis();
    server.send(200, "application/json", "{\"ok\":true}");
  });
  server.on("/api/reboot", HTTP_POST, []() {
    server.send(200, "application/json", "{\"ok\":true}");
    scheduleReboot(800);
  });
  server.on("/api/forget", HTTP_POST, []() {
    cfg.ssid[0] = 0;
    cfg.pass[0] = 0;
    saveConfig();
    server.send(200, "application/json", "{\"ok\":true}");
    scheduleReboot(800);
  });
  server.on("/api/photos", HTTP_GET, handlePhotoList);
  server.on("/photo", HTTP_GET, handlePhotoGet);
  server.on("/api/photo", HTTP_POST, handlePhotoDone, handlePhotoUpload);
  server.on("/api/photo/delete", HTTP_POST, handlePhotoDelete);
  server.onNotFound(handleNotFound);
  if (!apMode) httpUpdater.setup(&server, "/update");
  server.begin();
}

// ------------------------------------------------------------------
//  Wi-Fi
// ------------------------------------------------------------------
void startAP() {
  apMode = true;
  apStart = millis();
  apLastClient = millis();
  WiFi.disconnect();
  WiFi.mode(WIFI_AP_STA);
  IPAddress ip(192, 168, 4, 1);
  WiFi.softAPConfig(ip, ip, IPAddress(255, 255, 255, 0));
  WiFi.softAP(AP_SSID, AP_PASS);
  dns.setErrorReplyCode(DNSReplyCode::NoError);
  dns.start(53, "*", ip);
  WiFi.scanNetworks(true, true);  // scansione in anticipo: la pagina trova gia' le reti
  drawApScreen();
  Serial.println("Modalita' setup: rete SmallTV-Setup, pagina 192.168.4.1");
}

bool connectWifi() {
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.hostname(HOSTNAME);
  WiFi.setAutoReconnect(true);
  WiFi.begin(cfg.ssid, cfg.pass);
  uint32_t t0 = millis();
  int dots = 0;
  char msg[40];
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 25000UL) {
    delay(250);
    if (((millis() - t0) / 500) != (uint32_t)dots) {
      dots = (int)((millis() - t0) / 500);
      snprintf(msg, sizeof(msg), "Connessione%.*s", (dots % 4), "...");
      drawSplash(msg);
    }
  }
  return WiFi.status() == WL_CONNECTED;
}

// ------------------------------------------------------------------
//  setup / loop
// ------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("SmallTV Ultra " FW_VERSION);

  analogWriteRange(255);
  pinMode(BL_PIN, OUTPUT);
  setBacklight(0);

  if (!LittleFS.begin()) {
    LittleFS.format();
    LittleFS.begin();
  }
  loadConfig();
  th = &THEMES[cfg.theme];

  tft.init();
  tft.setSwapBytes(true);  // per pushImage (foto): colori RGB565 nativi
  scanPhotos();
  tft.setRotation(cfg.flip ? 2 : 0);
  tft.invertDisplay(cfg.invert);
  tft.fillScreen(col(th->bg));
  initTimeFont();
  applyBrightness(true);
  drawSplash("Avvio...");

  bool online = false;
  if (cfg.ssid[0] != 0) online = connectWifi();

  if (!online) {
    startAP();
  } else {
    Serial.print("Connesso, IP ");
    Serial.println(WiFi.localIP());
    configTime(TZ_ROME, "pool.ntp.org", "time.cloudflare.com");
    if (MDNS.begin(HOSTNAME)) MDNS.addService("http", "tcp", 80);
    drawIpScreen(WiFi.localIP().toString().c_str());
    delay(3500);
    fullRedraw();
    screen = SCR_MAIN;
    screenStart = millis();
  }
  setupWebRoutes();
}

void loop() {
  server.handleClient();

  if (pendingReboot && (int32_t)(millis() - rebootAt) >= 0) {
    delay(100);
    ESP.restart();
  }

  if (apMode) {
    dns.processNextRequest();
    if (WiFi.softAPgetStationNum() > 0) apLastClient = millis();
    // rete salvata ma non raggiunta: dopo 3 minuti senza nessuno collegato riprovo
    if (cfg.ssid[0] != 0 && (millis() - apLastClient) > 180000UL) ESP.restart();
    delay(2);
    return;
  }

  MDNS.update();

  if (needRedraw) {
    needRedraw = false;
    if (screen == SCR_MAIN) fullRedraw();
    else enterMain();  // impostazioni schermo cambiate: torno alla principale (ridisegna tutto)
  }

  if (WiFi.status() == WL_CONNECTED && (int32_t)(millis() - nextWx) >= 0) {
    bool ok = fetchWeather();
    nextWx = millis() + (ok ? (uint32_t)cfg.wxMinutes * 60000UL : 60000UL);
  }

  const uint32_t elapsed = millis() - screenStart;
  if (screen == SCR_MAIN) {
    updateClock();
    if (wxDirty) {
      wxDirty = false;
      drawCard();
    }

    // dato a rotazione nella riga delle condizioni (ogni 4 secondi)
    if (wx.valid && (millis() - lastInfoRot) >= INFO_ROTATE_MS) {
      lastInfoRot = millis();
      nextInfo();
      drawInfoLine();
    }
    if (elapsed >= MAIN_MS) advance(SCR_MAIN);
  } else if (elapsed >= screenMs(screen)) {
    advance(screen);
  }

  // luminosita' giorno/notte
  if ((millis() - lastBrightCheck) >= 1000UL) {
    lastBrightCheck = millis();
    applyBrightness(false);
  }
  delay(2);
}
