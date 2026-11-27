// Calcoli lunari (senza librerie): illuminazione, eta' e prossima luna piena.
// Formule: Jean Meeus, "Astronomical Algorithms" (cap. 47 semplificato e cap. 49).
#pragma once
#include <math.h>
#include <stdint.h>
#include <time.h>

namespace moon {

static const double PI_ = 3.14159265358979323846;
static inline double rad(double d) { return d * PI_ / 180.0; }
static inline double norm360(double d) {
  d = fmod(d, 360.0);
  return d < 0 ? d + 360.0 : d;
}

static inline double jdFromUnix(double t) { return t / 86400.0 + 2440587.5; }
static inline double unixFromJd(double jd) { return (jd - 2440587.5) * 86400.0; }

struct Phase {
  double elong;  // elongazione Luna-Sole, gradi 0..360 (0 nuova, 180 piena)
  double lit;    // frazione illuminata 0..1
  double age;    // eta' in giorni (0..29.53)
};

// Fase lunare all'istante unix t (UTC)
static inline Phase phaseAt(double t) {
  const double jd = jdFromUnix(t);
  const double T = (jd - 2451545.0) / 36525.0;
  const double Lp = norm360(218.3164477 + 481267.88123421 * T);  // longitudine media Luna
  const double D = norm360(297.8501921 + 445267.1114034 * T);    // elongazione media
  const double M = norm360(357.5291092 + 35999.0502909 * T);     // anomalia media Sole
  const double Mp = norm360(134.9633964 + 477198.8675055 * T);   // anomalia media Luna
  const double F = norm360(93.2720950 + 483202.0175233 * T);     // argomento di latitudine
  const double L0 = norm360(280.46646 + 36000.76983 * T);        // longitudine media Sole

  double lonMoon = Lp + 6.289 * sin(rad(Mp)) + 1.274 * sin(rad(2 * D - Mp)) + 0.658 * sin(rad(2 * D)) +
                   0.214 * sin(rad(2 * Mp)) - 0.186 * sin(rad(M)) - 0.114 * sin(rad(2 * F)) +
                   0.059 * sin(rad(2 * D - 2 * Mp)) + 0.057 * sin(rad(2 * D - M - Mp)) +
                   0.053 * sin(rad(2 * D + Mp)) + 0.046 * sin(rad(2 * D - M)) + 0.041 * sin(rad(Mp - M)) -
                   0.035 * sin(rad(D)) - 0.030 * sin(rad(Mp + M));
  double lonSun = L0 + 1.914602 * sin(rad(M)) + 0.019993 * sin(rad(2 * M));

  Phase p;
  p.elong = norm360(lonMoon - lonSun);
  p.lit = (1.0 - cos(rad(p.elong))) / 2.0;
  p.age = p.elong / 360.0 * 29.530588861;
  return p;
}

// Istante (unix UTC) della luna piena numero k (k = intero + 0.5), Meeus cap. 49
static inline double fullMoonUnix(double k) {
  const double T = k / 1236.85;
  const double T2 = T * T, T3 = T2 * T, T4 = T3 * T;
  double jde = 2451550.09766 + 29.530588861 * k + 0.00015437 * T2 - 0.000000150 * T3 + 0.00000000073 * T4;
  const double E = 1 - 0.002516 * T - 0.0000074 * T2;
  const double M = rad(norm360(2.5534 + 29.10535670 * k - 0.0000014 * T2 - 0.00000011 * T3));
  const double Mp = rad(norm360(201.5643 + 385.81693528 * k + 0.0107582 * T2 + 0.00001238 * T3 - 0.000000058 * T4));
  const double F = rad(norm360(160.7108 + 390.67050284 * k - 0.0016118 * T2 - 0.00000227 * T3 + 0.000000011 * T4));
  const double Om = rad(norm360(124.7746 - 1.56375588 * k + 0.0020672 * T2 + 0.00000215 * T3));
  jde += -0.40614 * sin(Mp) + 0.17302 * E * sin(M) + 0.01614 * sin(2 * Mp) + 0.01043 * sin(2 * F) +
         0.00734 * E * sin(Mp - M) - 0.00515 * E * sin(Mp + M) + 0.00209 * E * E * sin(2 * M) -
         0.00111 * sin(Mp - 2 * F) - 0.00057 * sin(Mp + 2 * F) + 0.00056 * E * sin(2 * Mp + M) -
         0.00042 * sin(3 * Mp) + 0.00042 * E * sin(M + 2 * F) + 0.00038 * E * sin(M - 2 * F) -
         0.00024 * E * sin(2 * Mp - M) - 0.00017 * sin(Om) - 0.00007 * sin(Mp + 2 * M) +
         0.00004 * sin(2 * Mp - 2 * F) + 0.00004 * sin(3 * M) + 0.00003 * sin(Mp + M - 2 * F) +
         0.00003 * sin(2 * Mp + 2 * F) - 0.00003 * sin(Mp + M + 2 * F) + 0.00003 * sin(Mp - M + 2 * F) -
         0.00002 * sin(Mp - M - 2 * F) - 0.00002 * sin(3 * Mp + M) + 0.00002 * sin(4 * Mp);
  // da tempo dinamico (TT) a UTC: DeltaT circa 69 s nel 2026
  return unixFromJd(jde) - 69.0;
}

// Prossima luna piena dopo l'istante unix t (UTC)
static inline double nextFullMoon(double t) {
  const double jd = jdFromUnix(t);
  double k = floor((jd - 2451550.09766) / 29.530588861) + 0.5;
  for (int i = 0; i < 4; i++) {
    double f = fullMoonUnix(k);
    if (f > t) {
      // controllo che non ci sia una piena precedente ancora successiva a t
      double prev = fullMoonUnix(k - 1);
      if (prev > t) {
        k -= 1;
        continue;
      }
      return f;
    }
    k += 1;
  }
  return fullMoonUnix(k);
}

// Nome della fase (senza accenti: font del display)
static inline const char* phaseName(const Phase& p) {
  const double e = p.elong;
  if (e < 11.0 || e >= 349.0) return "Luna nuova";
  if (e < 79.0) return "Luna crescente";
  if (e < 101.0) return "Primo quarto";
  if (e < 169.0) return "Gibbosa crescente";
  if (e < 191.0) return "Luna piena";
  if (e < 259.0) return "Gibbosa calante";
  if (e < 281.0) return "Ultimo quarto";
  return "Luna calante";
}

}  // namespace moon
