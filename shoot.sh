#!/usr/bin/env bash
# Lancia calcC, lo rende floating, ne fa uno screenshot e lo chiude.
# Uso: shoot.sh <binary> <output.png> [sleep_secondi]
set -u
BIN="$1"; OUT="$2"; SLEEP="${3:-4}"

pkill -x calcC 2>/dev/null
sleep 0.4

setsid "$BIN" >/tmp/calcc-run.log 2>&1 &
PID=$!
sleep "$SLEEP"

find_win() {
    niri msg windows -j 2>/dev/null | python3 -c '
import json,sys
try: ws=json.load(sys.stdin)
except Exception: sys.exit(0)
for w in ws:
    a=(w.get("app_id") or "").lower(); t=(w.get("title") or "").lower()
    if a=="calcc" or "calculator" in t:
        print(w["id"]); break
'
}

if command -v niri >/dev/null 2>&1; then
    WID="$(find_win)"
    if [ -n "${WID:-}" ]; then
        niri msg action focus-window --id "$WID" >/dev/null 2>&1
        niri msg action toggle-window-floating --id "$WID" >/dev/null 2>&1
        sleep 1.5
    else
        echo "finestra calcC non trovata in niri"
    fi
fi

grim "$OUT" 2>&1 && echo "SCREENSHOT OK -> $OUT" || echo "SCREENSHOT FALLITA"
ls -la "$OUT" 2>/dev/null

sleep 0.3
kill "$PID" 2>/dev/null
sleep 0.3
pkill -x calcC 2>/dev/null
exit 0
