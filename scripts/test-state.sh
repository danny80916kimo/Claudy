#!/bin/bash
# Cycle through all states to verify the display rendering on the device.
set -euo pipefail

RAW_URL="${CLAUDY_URL:-http://claudy.local/state}"
AUTH="${CLAUDY_TOKEN:-}"
IP_CACHE="$HOME/.claude/claudy_ip_cache"

# Reuse the IP cache from send_state.py so we don't depend on flaky system mDNS.
resolve_url() {
  local url="$1"
  case "$url" in
    *".local"*)
      if [ -f "$IP_CACHE" ] && [ $(( $(date +%s) - $(stat -f %m "$IP_CACHE") )) -lt 60 ]; then
        local ip
        ip=$(cat "$IP_CACHE")
        if [ -n "$ip" ]; then
          echo "$url" | sed "s|://[^:/]*|://$ip|"
          return
        fi
      fi
      ;;
  esac
  echo "$url"
}

URL="$(resolve_url "$RAW_URL")"

send() {
  local payload="$1"
  if [ -n "$AUTH" ]; then
    curl -sS --max-time 2 -H "Content-Type: application/json" -H "X-Claudy-Token: $AUTH" -X POST -d "$payload" "$URL" || true
  else
    curl -sS --max-time 2 -H "Content-Type: application/json" -X POST -d "$payload" "$URL" || true
  fi
  echo
}

echo "==> POST $URL"
send '{"state":"idle","message":"Hello from test"}'; sleep 2
send '{"state":"thinking","message":"Reading some files..."}'; sleep 2
send '{"state":"working","tool":"Bash","message":"npm run build","tokens":{"used":42000,"max":200000}}'; sleep 2
send '{"state":"working","tool":"Edit","message":"firmware/firmware.ino","tokens":{"used":85000,"max":200000}}'; sleep 2
send '{"state":"working","tool":"Grep","message":"pattern: TODO","tokens":{"used":110000,"max":200000}}'; sleep 2
send '{"state":"waiting","message":"Approve Bash command?","tokens":{"used":150000,"max":200000}}'; sleep 2
send '{"state":"error","message":"Compilation failed","tokens":{"used":180000,"max":200000}}'; sleep 2
send '{"state":"done","message":"Task complete!"}'; sleep 2
send '{"state":"idle","message":"Idle"}'
