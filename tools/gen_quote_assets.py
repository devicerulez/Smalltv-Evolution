#!/usr/bin/env python3
"""Genera smalltv-ultra-device/src/qfonts.h (font bitmap anti-aliased 4 bit con accenti) e smalltv-ultra-device/src/quotes.h (citazioni).
Uso:  python3 tools/gen_quote_assets.py  [cartella_font]
Serve Pillow e i font: CrimsonPro-Italic.ttf, CrimsonPro-Regular.ttf, InstrumentSans-Bold.ttf
(Open Font License). Il firmware gia' contiene i file generati: questo script serve solo
per cambiare le citazioni o i font."""
import os, sys
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, "..", "smalltv-ultra-device", "src")
FONT_DIR = sys.argv[1] if len(sys.argv) > 1 else HERE

# (testo, autore)  -- solo citazioni verificabili; gli apostrofi diventano tipografici
QUOTES = [
 ("Fatti non foste a viver come bruti, ma per seguir virtute e canoscenza.", "Dante Alighieri"),
 ("Nel mezzo del cammin di nostra vita mi ritrovai per una selva oscura.", "Dante Alighieri"),
 ("E quindi uscimmo a riveder le stelle.", "Dante Alighieri"),
 ("L'amor che move il sole e l'altre stelle.", "Dante Alighieri"),
 ("Non ragioniam di lor, ma guarda e passa.", "Dante Alighieri"),
 ("Il naufragar m'è dolce in questo mare.", "Giacomo Leopardi"),
 ("Sempre caro mi fu quest'ermo colle, e questa siepe, che da tanta parte dell'ultimo orizzonte il guardo esclude.", "Giacomo Leopardi"),
 ("Che fai tu, luna, in ciel? dimmi, che fai, silenziosa luna?", "Giacomo Leopardi"),
 ("O graziosa luna, io mi rammento che, or volge l'anno, sovra questo colle io venia pien d'angoscia a rimirarti.", "Giacomo Leopardi"),
 ("Laudato si', mi' Signore, per sora Luna e le stelle: in celu l'ài formate clarite et pretiose et belle.", "San Francesco d'Assisi"),
 ("M'illumino d'immenso.", "Giuseppe Ungaretti"),
 ("Ognuno sta solo sul cuor della terra trafitto da un raggio di sole: ed è subito sera.", "Salvatore Quasimodo"),
 ("Un paese ci vuole, non fosse che per il gusto di andarsene via.", "Cesare Pavese"),
 ("Il coraggio, uno, se non ce l'ha, mica se lo può dare.", "Alessandro Manzoni"),
 ("Addio, monti sorgenti dall'acque, ed elevati al cielo.", "Alessandro Manzoni"),
 ("Se comprendere è impossibile, conoscere è necessario.", "Primo Levi"),
 ("Cercare e saper riconoscere chi e cosa, in mezzo all'inferno, non è inferno, e farlo durare, e dargli spazio.", "Italo Calvino"),
 ("La leggerezza per me si associa con la precisione e la determinazione, non con la vaghezza e l'abbandono al caso.", "Italo Calvino"),
 ("Dai diamanti non nasce niente, dal letame nascono i fior.", "Fabrizio De André"),
 ("Istruitevi, perché avremo bisogno di tutta la vostra intelligenza.", "Antonio Gramsci"),
 ("Gli uomini passano, le idee restano.", "Giovanni Falcone"),
 ("Chi poco pensa, molto erra.", "Leonardo da Vinci"),
 ("La filosofia è scritta in questo grandissimo libro che continuamente ci sta aperto innanzi agli occhi, io dico l'universo.", "Galileo Galilei"),
 ("Non abbiamo poco tempo, ma ne abbiamo perduto molto.", "Seneca"),
 ("Mentre rimandiamo, la vita corre.", "Seneca"),
 ("Non c'è vento favorevole per il marinaio che non sa dove andare.", "Seneca"),
 ("Ovunque ci sia un essere umano, c'è spazio per un gesto di bontà.", "Seneca"),
 ("Ciò che non giova all'alveare non giova nemmeno all'ape.", "Marco Aurelio"),
 ("Non sono le cose a turbare gli uomini, ma le opinioni che essi hanno delle cose.", "Epitteto"),
 ("L'uomo è per natura un animale politico.", "Aristotele"),
 ("Una vita senza ricerca non è degna di essere vissuta.", "Socrate"),
 ("Non ci si può bagnare due volte nello stesso fiume.", "Eraclito"),
 ("Ciò che non desideri per te, non farlo agli altri.", "Confucio"),
 ("Un viaggio di mille miglia comincia con un singolo passo.", "Lao Tzu"),
 ("Se possiedi un giardino e una biblioteca, non ti mancherà nulla.", "Cicerone"),
 ("Forse un giorno ci piacerà ricordare anche tutto questo.", "Virgilio"),
 ("L'amore vince ogni cosa: arrendiamoci anche noi all'amore.", "Virgilio"),
 ("Cogli l'attimo, confidando il meno possibile nel domani.", "Orazio"),
 ("La goccia scava la pietra, non con la forza, ma cadendo spesso.", "Ovidio"),
 ("Sono uomo: nulla di ciò che è umano mi è estraneo.", "Terenzio"),
 ("La mente non è un vaso da riempire, ma un fuoco da accendere.", "Plutarco"),
 ("Siamo fatti della stessa sostanza di cui sono fatti i sogni.", "William Shakespeare"),
 ("Ci sono più cose in cielo e in terra, Orazio, di quante ne sogni la tua filosofia.", "William Shakespeare"),
 ("Posso resistere a tutto, tranne che alle tentazioni.", "Oscar Wilde"),
 ("L'essenziale è invisibile agli occhi.", "Antoine de Saint-Exupéry"),
 ("È il tempo che hai perduto per la tua rosa che ha reso la tua rosa così importante.", "Antoine de Saint-Exupéry"),
 ("Il vero viaggio di scoperta non consiste nel cercare nuove terre, ma nell'avere nuovi occhi.", "Marcel Proust"),
 ("Chi ha un perché abbastanza forte può sopportare quasi ogni come.", "Friedrich Nietzsche"),
 ("Senza la musica la vita sarebbe un errore.", "Friedrich Nietzsche"),
 ("Sapere aude! Abbi il coraggio di servirti della tua intelligenza.", "Immanuel Kant"),
 ("Il cuore ha le sue ragioni, che la ragione non conosce.", "Blaise Pascal"),
 ("Non tutti quelli che vagano si sono perduti.", "J.R.R. Tolkien"),
 ("La speranza è quella cosa piumata che si posa sull'anima.", "Emily Dickinson"),
 ("Abbiate pazienza verso tutto ciò che è irrisolto nel vostro cuore.", "Rainer Maria Rilke"),
 ("Non basta sapere, si deve anche applicare; non basta volere, si deve anche fare.", "Johann Wolfgang Goethe"),
 ("L'immaginazione è più importante della conoscenza.", "Albert Einstein"),
 ("Siamo un modo per il cosmo di conoscere sé stesso.", "Carl Sagan"),
 ("Un piccolo passo per un uomo, un grande balzo per l'umanità.", "Neil Armstrong"),
 ("Che cosa so io?", "Michel de Montaigne"),
]

def typo(s):  # apostrofo tipografico
    return s.replace("'", "’")

TEXTS = [typo(t) for t, a in QUOTES]
AUTHORS = [typo(a).upper() for t, a in QUOTES]

def find(name):
    for d in (FONT_DIR, HERE, "/mnt/skills/examples/canvas-design/canvas-fonts"):
        p = os.path.join(d, name)
        if os.path.exists(p):
            return p
    sys.exit("font non trovato: " + name)

ascii_set = [chr(c) for c in range(32, 127)]
text_chars = sorted(set(ascii_set) | set("".join(TEXTS)) | set("“”’"))
auth_chars = sorted(set(ascii_set) | set("".join(AUTHORS)))
mark_chars = ["“", "”"]

def build(name, path, px, chars):
    f = ImageFont.truetype(path, px)
    asc, desc = f.getmetrics()
    cps, glyphs, bmp = [], [], bytearray()
    for ch in sorted(chars, key=ord):
        pad = px
        im = Image.new("L", (px * 3, px * 3), 0)
        ImageDraw.Draw(im).text((pad, pad * 2), ch, font=f, fill=255, anchor="ls")
        bb = im.getbbox()
        adv = int(round(f.getlength(ch) * 8))
        adv = max(0, min(255, adv))
        if bb is None:
            w = h = xo = yo = 0
            off = len(bmp)
        else:
            l, t, r, b = bb
            w, h = r - l, b - t
            xo, yo = l - pad, t - pad * 2
            off = len(bmp)
            px_ = im.crop(bb).load()
            nib = []
            for y in range(h):
                for x in range(w):
                    nib.append(min(15, (px_[x, y] * 15 + 127) // 255))
            if len(nib) % 2: nib.append(0)
            for i in range(0, len(nib), 2):
                bmp.append((nib[i] << 4) | nib[i + 1])
        assert off < 65536 and -128 <= xo < 128 and -128 <= yo < 128
        cps.append(ord(ch)); glyphs.append((off, w, h, xo, yo, adv))
    return dict(name=name, cps=cps, glyphs=glyphs, bmp=bytes(bmp), asc=asc, lh=asc + desc, px=px)

fonts = []
it = find("CrimsonPro-Italic.ttf")
for i, px in enumerate([30, 25, 21, 18, 15]):
    fonts.append(build("QF_T%d" % i, it, px, text_chars))
fonts.append(build("QF_AUTH", find("InstrumentSans-Bold.ttf"), 12, auth_chars))
fonts.append(build("QF_MARK", find("CrimsonPro-Regular.ttf"), 72, mark_chars))

def carr(vals, per=16):
    out = []
    for i in range(0, len(vals), per):
        out.append("  " + ",".join(str(v) for v in vals[i:i + per]) + ",")
    return "\n".join(out)

lines = ["// GENERATO da tools/gen_quote_assets.py - non modificare a mano.",
         "// Font: Crimson Pro (Italic/Regular) e Instrument Sans Bold, licenza SIL OFL.",
         "#pragma once", "#include <Arduino.h>", "#include <pgmspace.h>", "",
         "struct QGlyph { uint16_t off; uint8_t w, h; int8_t xo, yo; uint8_t adv8; };",
         "struct QFont { const uint16_t* cps; const QGlyph* g; const uint8_t* bmp; uint16_t n; uint8_t asc; uint8_t lh; };", ""]
total = 0
for F in fonts:
    n = F["name"]
    lines.append("static const uint16_t %s_cp[] PROGMEM = {\n%s\n};" % (n, carr(F["cps"], 12)))
    gl = []
    for g in F["glyphs"]:
        gl.append("{%d,%d,%d,%d,%d,%d}" % g)
    body = ",\n".join("  " + ",".join(gl[i:i + 6]) for i in range(0, len(gl), 6))
    lines.append("static const QGlyph %s_g[] PROGMEM = {\n%s\n};" % (n, body))
    lines.append("static const uint8_t %s_b[] PROGMEM = {\n%s\n};" % (n, carr(list(F["bmp"]), 24)))
    lines.append("static const QFont %s = {%s_cp, %s_g, %s_b, %d, %d, %d};\n" % (n, n, n, n, len(F["cps"]), F["asc"], F["lh"]))
    sz = len(F["bmp"]) + len(F["cps"]) * 2 + len(F["glyphs"]) * 8
    total += sz
    print("%-8s px=%-3d glifi=%-3d bytes=%d" % (n, F["px"], len(F["cps"]), sz))
lines.append("static const QFont* const QF_TEXT[] = {&QF_T0, &QF_T1, &QF_T2, &QF_T3, &QF_T4};")
lines.append("static const uint8_t QF_TEXT_N = 5;")
open(os.path.join(SRC, "qfonts.h"), "w", encoding="utf-8").write("\n".join(lines) + "\n")
print("totale font: %d byte" % total)

q = ["// GENERATO da tools/gen_quote_assets.py - non modificare a mano.",
     "#pragma once", "#include <Arduino.h>", "#include <pgmspace.h>", "",
     "struct Quote { const char* text; const char* author; };"]
for i, (t, a) in enumerate(zip(TEXTS, AUTHORS)):
    q.append('static const char QT_%d[] PROGMEM = "%s";' % (i, t.replace('"', '\\"')))
    q.append('static const char QA_%d[] PROGMEM = "%s";' % (i, a))
q.append("static const Quote QUOTES[] = {")
for i in range(len(TEXTS)):
    q.append("  {QT_%d, QA_%d}," % (i, i))
q.append("};")
q.append("static const uint16_t QUOTE_COUNT = %d;" % len(TEXTS))
open(os.path.join(SRC, "quotes.h"), "w", encoding="utf-8").write("\n".join(q) + "\n")
print("citazioni:", len(TEXTS))
