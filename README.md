# SmallTV Ultra Device (ESP8266) - Release V.1.0.1 Italiano - Orologio + Meteo

Firmware personalizzato per SmallTV Ultra (ESP8266, TFT ST7789 240x240, flash 4 MB).

- Orologio grande con barra dei secondi, data in italiano, ora automatica (NTP) con fuso **Europe/Rome** e ora legale/solare automatica
- Meteo con icone disegnate a codice, temperatura, min/max, umidita' (Open-Meteo, gratis, nessuna chiave API)
- La riga delle condizioni cambia ogni 4 secondi: condizioni, temperatura percepita, vento, pressione (Atm), alba, tramonto
- Data con anno, formato ora 24 h oppure 12 h (AM/PM) a scelta
- Modalita' notturna: luminosita' 0-100% in una fascia oraria a scelta
- 5 temi colore (Neon, Aurora, Tramonto, Ghiaccio, Ambra)
- **Portale Wi-Fi**: se non c'e' una rete salvata (o non si collega) il display mostra come configurarla dal telefono
- **Interfaccia web** su `http://smalltv.local` (o l'IP mostrato all'avvio): citta', tema, luminosita', rotazione, aggiornamento firmware

> Testato su hardware reale: i pin sono quelli piu' comuni per questo dispositivo. Vedi "Se qualcosa non va".

## 1. Backup del firmware originale (consigliato!)
Collega il dispositivo via USB e con `esptool` (`pip install esptool`):
```
esptool.py --port COMX read_flash 0 0x400000 backup-originale.bin
```
Per ripristinare il firmware originale: `esptool.py --port COMX write_flash 0 backup-originale.bin`.

## 2. Installare il firmware
Via USB:
```
esptool.py --port COMX --baud 460800 write_flash 0x0 smalltv-ultra-firmware1.0.0.bin
```
A seconda del modello puo' servire mettere GPIO0 a GND all'accensione per entrare in modalita' bootloader.
Se il firmware originale ha una pagina di aggiornamento via browser, puoi provare a caricare il `.bin` da li'; dopo la prima installazione gli aggiornamenti si fanno da `http://smalltv.local/update`.

## 3. Primo avvio
1. Il display mostra **CONFIGURA WI-FI**: collegati alla rete `SmallTV-Setup` (password `smalltv1234`).
2. Si apre da sola la pagina di setup (altrimenti vai su `192.168.4.1`): scegli la rete e inserisci la password.
3. Lo SmallTV si riavvia, mostra l'indirizzo e passa all'orologio.
4. Dal telefono, ora collegato al tuo Wi-Fi, apri l'indirizzo segnalato al riavvio per configurare il dispositivo.

## Se qualcosa non va
| Sintomo | Cosa fare |
|---|---|
| Schermo nero ma il dispositivo e' acceso (la rete `SmallTV-Setup` compare) | Retroilluminazione: in `platformio.ini` cambia `-DBL_ACTIVE_LOW=1` in `0` e ricompila |
| Colori in negativo | Interfaccia web -> Schermo -> "Inverti colori" |
| Rosso e blu scambiati | Interfaccia web -> Schermo -> "Scambia rosso/blu" |
| Immagine capovolta | Interfaccia web -> Schermo -> "Ruota di 180" |
| Immagine disturbata / righe | In `platformio.ini` abbassa `-DSPI_FREQUENCY=40000000` a `27000000` |
| Immagine spostata o con bordo | Prova `-DCGRAM_OFFSET=1` (gia' attivo) togliendolo, oppure ruota di 180 |
| Meteo "Carico meteo..." per sempre | Controlla la connessione a internet; dal monitor seriale (115200) vedi il motivo |

## Pin usati (ESP8266)
SCLK = GPIO14, MOSI = GPIO13, DC = GPIO0, RST = GPIO2, CS = non usato, retroilluminazione = GPIO5 (PWM, attiva bassa).

## Note
- La rete `SmallTV-Setup` e' protetta; la password e' mostrata sul display. Puoi cambiarla in `src/main.cpp` (`AP_PASS`).
- La pagina `/update` non ha password: usala solo su una rete di cui ti fidi.
- Il meteo e' aggiornato ogni 10-60 minuti (a scelta). Durante l'aggiornamento l'orologio puo' fermarsi per 1-3 secondi.
- Le lettere accentate non sono visualizzate sul display (font solo ASCII): "Forli" invece di "Forli'".

## Versioni
- **V.1.0.1** - anno nella data; citta' sempre in primo piano; rimosso il pallino di stato del meteo; riga condizioni a rotazione (percepita, vento, pressione, alba, tramonto); modalita' notturna; formato ora 12/24 h.
- **V.1.0.0** - prima versione.
