#!/bin/bash
# Live poll of the device state. Ctrl-C to quit.
set -u

URL="${CLAUDY_URL:-http://claudy.local/state}"

# If claudy.local doesn't resolve in 1s, fall back to dns-sd lookup.
if ! getent hosts "$(echo "$URL" | sed -E 's|https?://||;s|/.*||')" >/dev/null 2>&1; then
  if [[ "$URL" == *"claudy.local"* ]]; then
    IP=$(dns-sd -G v4 claudy.local 2>/dev/null | awk '/Add/ && $7 ~ /^[0-9]+\./ {print $7; exit}' &) ; DPID=$!
    sleep 1.5
    kill $DPID 2>/dev/null
    if [ -n "${IP:-}" ]; then
      URL="${URL//claudy.local/$IP}"
      echo "(resolved claudy.local -> $IP)"
    fi
  fi
fi

echo "Polling $URL  (Ctrl-C to quit)"
echo
while true; do
  TS=$(date +"%H:%M:%S")
  RESP=$(curl -sS --max-time 2 "$URL" 2>/dev/null || echo "{}")
  STATE=$(echo "$RESP"   | python3 -c 'import sys,json; d=json.load(sys.stdin); print(d.get("state","?"))' 2>/dev/null)
  TOOL=$(echo "$RESP"    | python3 -c 'import sys,json; d=json.load(sys.stdin); print(d.get("tool","") or "-")' 2>/dev/null)
  MSG=$(echo "$RESP"     | python3 -c 'import sys,json; d=json.load(sys.stdin); print(d.get("message",""))' 2>/dev/null)
  UPT=$(echo "$RESP"     | python3 -c 'import sys,json; d=json.load(sys.stdin); print(d.get("uptime_ms",0)//1000)' 2>/dev/null)
  TOK_U=$(echo "$RESP"   | python3 -c 'import sys,json; d=json.load(sys.stdin); print(d.get("tokens",{}).get("used",0))' 2>/dev/null)
  TOK_M=$(echo "$RESP"   | python3 -c 'import sys,json; d=json.load(sys.stdin); print(d.get("tokens",{}).get("max",0))' 2>/dev/null)
  printf "\r[%s] %-9s tool=%-8s msg=%-40.40s up=%ss tok=%s/%s    " \
    "$TS" "${STATE:-???}" "${TOOL:- -}" "${MSG:-}" "${UPT:-0}" "${TOK_U:-0}" "${TOK_M:-0}"
  sleep 1
done
