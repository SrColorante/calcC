#!/usr/bin/env bash
# Lancia calcC e ne salva uno screenshot.
# Uso: shoot.sh <binary> <output.png> [sleep] [frame] ["chiavi"]
#
# Due metodi, in quest'ordine:
#   1) CALCC_SHOT: la finestra stessa si fotogramma il framebuffer GL.
#      Cattura SOLO l'app, senza chrome del compositor, e funziona
#      anche dove grim non e' installato.
#   2) grim: cattura l'intero schermo, incluso tutto quello che c'e'
#      intorno. E' il ripiego, e su un compositor a tiling riprende
#      anche le altre finestre: i due metodi non sono equivalenti.
set -u
BIN="$1"; OUT="$2"; SLEEP="${3:-3}"; FRAME="${4:-30}"; KEYS="${5:-}"
DIR="$(cd "$(dirname "$OUT")" && pwd)"
mkdir -p "$DIR"
# raylib prende solo il nome del file e lo scrive nel base path della
# finestra, quindi un percorso assoluto verrebbe ignorato: l'app va
# avviata con la directory di destinazione come cwd.
BIN="$(cd "$(dirname "$BIN")" && pwd)/$(basename "$BIN")"
OUT="$DIR/$(basename "$OUT")"
rm -f "$OUT"

pkill -x calcC 2>/dev/null
sleep 0.4

setsid env -C "$DIR" CALCC_SHOT="$(basename "$OUT")" CALCC_SHOT_FRAME="$FRAME" \
    CALCC_SHOT_KEYS="$KEYS" "$BIN" >/tmp/calcc-run.log 2>&1 &
PID=$!
sleep "$SLEEP"

# La finestra va resa floating altrimenti il WM potrebbe tile-darla
# e ridimensionarla: su niri questo e' l'unica manipolazione utile.
if command -v niri >/dev/null 2>&1; then
    WID="$(niri msg windows -j 2>/dev/null | python3 -c '
import json,sys
try: ws=json.load(sys.stdin)
except Exception: sys.exit(0)
for w in ws:
    a=(w.get("app_id") or "").lower(); t=(w.get("title") or "").lower()
    if a=="calcc" or "calculator" in t:
        print(w["id"]); break
' 2>/dev/null)"
    if [ -n "${WID:-}" ]; then
        niri msg action toggle-window-floating --id "$WID" >/dev/null 2>&1
    fi
fi

kill "$PID" 2>/dev/null
sleep 0.3
pkill -x calcC 2>/dev/null

if [ -s "$OUT" ]; then
    echo "SCREENSHOT OK -> $OUT (framebuffer della finestra)"
else
    echo "CALCC_SHOT non ha prodotto nulla, ripiego su grim"
    (cd "$DIR" && setsid "$BIN" >/tmp/calcc-run.log 2>&1 &)
    sleep 1.5
    grim "$OUT" 2>/dev/null && echo "SCREENSHOT OK -> $OUT (schermo intero)" \
                             || echo "SCREENSHOT FALLITA (grim mancante?)"
    pkill -x calcC 2>/dev/null
fi
ls -la "$OUT" 2>/dev/null
exit 0
