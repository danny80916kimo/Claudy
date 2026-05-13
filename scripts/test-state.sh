#!/bin/bash
# Cycle through all states to verify the display rendering on the device.
set -euo pipefail

URL="${CLAUDY_URL:-http://claudy.local/state}"
AUTH="${CLAUDY_TOKEN:-}"

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
