#!/bin/bash
# Removes Claudy hook entries from ~/.claude/settings.json.
# Other hooks (e.g. GitKraken) are left untouched.

set -euo pipefail

SETTINGS="${CLAUDE_SETTINGS:-$HOME/.claude/settings.json}"
HERE="$(cd "$(dirname "$0")" && pwd)"
BRIDGE_CMD="$HERE/send-state.sh"

if [ ! -f "$SETTINGS" ]; then
  echo "settings.json not found at $SETTINGS"
  exit 1
fi

BACKUP="$SETTINGS.bak.$(date +%Y%m%d%H%M%S)"
cp "$SETTINGS" "$BACKUP"
echo "Backed up $SETTINGS -> $BACKUP"

python3 - "$SETTINGS" "$BRIDGE_CMD" <<'PY'
import json, sys, pathlib
settings_path = pathlib.Path(sys.argv[1])
cmd           = sys.argv[2]

data = json.loads(settings_path.read_text())
hooks = data.get("hooks", {})
removed = 0
for ev, entries in list(hooks.items()):
    for e in entries:
        before = len(e.get("hooks", []))
        e["hooks"] = [h for h in e.get("hooks", []) if h.get("command") != cmd]
        removed += before - len(e["hooks"])
    # cleanup empty groups
    hooks[ev] = [e for e in entries if e.get("hooks")]

settings_path.write_text(json.dumps(data, indent=2) + "\n")
print(f"Removed {removed} Claudy hook entries.")
PY
