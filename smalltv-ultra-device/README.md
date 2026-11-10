# SmallTV Evolution (ESP8266) - Release V.1.0.6 Italiano - Orologio + Meteo + Aria + Foto + Fasi lunari + Citazione del giorno + Countdown + Cripto + Numeri + Frattali

Firmware personalizzato per SmallTV Evolution (ESP8266, TFT ST7789 240x240, flash 4 MB).

- Orologio grande con barra dei secondi, data in italiano, ora automatica (NTP) con fuso **Europe/Rome** e ora legale/solare automatica
- Meteo con icone disegnate a codice, temperatura, min/max, umidita' (Open-Meteo, gratis, nessuna chiave API)
- La riga delle condizioni cambia ogni 4 secondi: condizioni, temperatura percepita, vento, pressione (Atm), alba, tramonto
- Data con anno, formato ora 24 h oppure 12 h (AM/PM) a scelta
- Modalita' notturna: luminosita' 0-100% in una fascia oraria a scelta
- **Previsioni dei prossimi 3 giorni** (schermata dedicata, durata a scelta)
- **Foto**: si caricano dall'interfaccia web, vengono convertite a 240x240 e compresse nel browser (8-15 KB ciascuna); fino a 20 foto, mostrate a rotazione con durata a scelta
- **Aria**: indice europeo di qualita' dell'aria (EAQI) su un quadrante a 6 fasce colorate con indicatore, livello di inquinamento a parole (ottima, buona, discreta, scadente, molto scadente, pessima) e valori di PM2.5 e PM10 con barretta colorata; dati Open-Meteo per la citta' impostata, aggiornati da soli ogni 30 minuti (nessuna chiave API)
- **Fasi lunari**: disco lunare disegnato in tempo reale, percentuale di illuminazione, nome della fase e data della prossima luna piena (calcolo astronomico a bordo, nessun servizio online)
- **Citazione del giorno**: 59 citazioni verificate (in italiano), una diversa ogni giorno, con font serif corsivo anti-aliased e lettere accentate
- **Countdown**: conto alla rovescia per vacanze, compleanni, Natale o qualsiasi evento; fino a 3 eventi con nome (max 16 caratteri) e data scelti nell'interfaccia web, mostrati a rotazione; l'anello si riempie man mano che l'evento si avvicina, il giorno stesso compare "OGGI!" con i coriandoli; con "ogni anno" l'evento si ripete (compleanni, Natale), altrimenti dopo la data viene saltato
- **Cripto**: grafici delle criptovalute (ultime 24 ore) con prezzo, variazione e riempimento sfumato; fino a 4 monete a scelta tra BTC, ETH, BNB, SOL, XRP, ADA, DOGE, LTC, LINK, AVAX, in euro o dollari (dati Binance, nessuna chiave API)
- **Numeri** 🍀: i numeri fortunati del giorno, scelti a caso da 1 a 90 e tutti diversi tra loro (gli stessi per tutto il giorno, cambiano a mezzanotte): 5 numeri su palline colorate in rilievo e, separato, il **numero Jolly** in oro con alone luminoso
- **Frattali**: Mandelbrot e Julia disegnati in tempo reale sul display, con aritmetica a virgola fissa, colorazione continua e palette neon; l'immagine si forma a blocchi sempre piu' fini. Alcune inquadrature sono animate ("Julia in movimento": la costante cambia e i colori scorrono). Ogni volta compare un frattale diverso
- **Ordine delle schermate a scelta**: nell'interfaccia web trascina i blocchi (mouse o dito) o usa le frecce da tastiera sulla maniglia; la principale (24 s) e' sempre la prima, poi le schermate attive nell'ordine scelto (default: previsioni, aria, luna, citazione, countdown, cripto, numeri, frattali, foto); le disattivate vengono saltate; una sola foto per giro
- **Ripristino di fabbrica** dal pulsante in fondo (Sistema): cancella impostazioni, foto e rete Wi-Fi e riparte dal portale di configurazione
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
esptool.py --port COMX --baud 460800 write_flash 0x0 smalltv-evolution-firmwarex.x.x.bin
```
A seconda del modello puo' servire mettere GPIO0 a GND all'accensione per entrare in modalita' bootloader.
Se il firmware originale ha una pagina di aggiornamento via browser, puoi provare a caricare il `.bin` da li'; dopo la prima installazione gli aggiornamenti si fanno da `http://smalltv.local/update`.

## 3. Primo avvio
1. Il display mostra **CONFIGURA WI-FI**: collegati alla rete `SmallTV-Setup` (password `smalltv1234`).
2. Si apre da sola la pagina di setup (altrimenti vai su `192.168.4.1`): scegli la rete e inserisci la password.
3. Lo SmallTV si riavvia, mostra l'indirizzo e passa all'orologio.
4. Dal telefono o da altro dispositivo, ora collegato al tuo Wi-Fi, apri l'indirizzo `http://smalltv.local` o l'ip segnalato al riavvio per configurare lo SmallTV Evolution.

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
- Le foto sono salvate nella memoria del dispositivo (circa 1 MB disponibile) e restano anche dopo il riavvio e gli aggiornamenti del firmware.
- Le lettere accentate nella citazione sono visualizzate correttamente; nel resto dell'interfaccia sul display (font di serie, solo ASCII) no: "Forli" invece di "Forli'".
- La schermata Cripto scarica i dati una moneta alla volta (circa ogni 5 minuti per moneta); se Binance non e' raggiungibile dalla tua rete la schermata viene saltata. Se un'altra moneta non compare, la coppia potrebbe non esistere nella valuta scelta.
- La schermata Aria scarica i dati da Open-Meteo solo se e' attiva; se per la tua zona non ci sono misure la schermata viene saltata. I valori PM sono in ug/m3 (microgrammi per metro cubo).
- Countdown: le date si inseriscono dall'interfaccia web (calendario del telefono); se un evento e' gia' passato e non si ripete ogni anno, viene saltato.
- Per cambiare le citazioni modifica `tools/gen_quote_assets.py` e rilancialo (`python3 tools/gen_quote_assets.py`, serve Pillow): rigenera `smalltv-ultra-device/src/quotes.h` e `smalltv-ultra-device/src/qfonts.h`.

## Versioni
- **V.1.0.6** - nuove schermate **Aria** (qualita' dell'aria, PM2.5, PM10, livello di inquinamento, aggiornamento automatico) e **Countdown** (conto alla rovescia per eventi con nome e data), attivabili dall'interfaccia web con durata 2-60 s (default 5); inserite di default Aria dopo Previsioni e prima di Fasi lunari, Countdown dopo Citazione e prima di Cripto e Numeri.
- **V.1.0.5** - il progetto si chiama ora **SmallTV Evolution** (schermata di avvio, interfaccia web, README, nome del `.bin`); nuove schermate **Numeri** 🍀 (numeri fortunati del giorno con numero Jolly) e **Frattali** (Mandelbrot e Julia in tempo reale), attivabili dall'interfaccia web con durata 2-60 s (default 5) e inserite di default subito prima di Foto.
- **V.1.0.4** - schermata Cripto (criptovalute); ordine delle schermate con trascinamento; ripristino di fabbrica dall'interfaccia web.
- **V.1.0.3** - schermata fasi lunari (illuminazione, fase, prossima luna piena); schermata citazione del giorno con font personalizzati; entrambe attivabili dall'interfaccia web con durata 2-60 s (default 5) e inserite nella sequenza.
- **V.1.0.2** - schermata previsioni 3 giorni; caricamento e visualizzazione foto (decoder JPEG integrato); sequenza delle schermate configurabile dall'interfaccia web.
- **V.1.0.1** - anno nella data; citta' sempre in primo piano; rimosso il pallino di stato del meteo; riga condizioni a rotazione (percepita, vento, pressione, alba, tramonto); modalita' notturna; formato ora 12/24 h.
- **V.1.0.0** - prima versione.
