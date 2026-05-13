#!/bin/bash
# Adds Claudy hook entries to ~/.claude/settings.json *alongside* any
# existing hooks. Idempotent — running it twice will not create duplicates.
# A timestamped backup is written before any change.

set -euo pipefail

SETTINGS="${CLAUDE_SETTINGS:-$HOME/.claude/settings.json}"
HERE="$(cd "$(dirname "$0")" && pwd)"
BRIDGE_CMD="$HERE/send-state.sh"

if [ ! -f "$SETTINGS" ]; then
  echo "settings.json not found at $SETTINGS"
  exit 1
fi
if [ ! -x "$BRIDGE_CMD" ]; then
  chmod +x "$BRIDGE_CMD"
fi

BACKUP="$SETTINGS.bak.$(date +%Y%m%d%H%M%S)"
cp "$SETTINGS" "$BACKUP"
echo "Backed up $SETTINGS -> $BACKUP"

python3 - "$SETTINGS" "$BRIDGE_CMD" <<'PY'
import json, sys, pathlib

settings_path = pathlib.Path(sys.argv[1])
cmd           = sys.argv[2]

events = [
    "SessionStart", "UserPromptSubmit",
    "PreToolUse", "PostToolUse", "PostToolUseFailure",
    "Notification", "PermissionRequest", "Elicitation",
    "Stop", "StopFailure", "PermissionDenied",
    "TaskCompleted", "SessionEnd",
]

data = json.loads(settings_path.read_text())
hooks = data.setdefault("hooks", {})

added = 0
for ev in events:
    entries = hooks.setdefault(ev, [])
    # Find or create a matcher="" group
    target = None
    for e in entries:
        if e.get("matcher", "") == "":
            target = e
            break
    if target is None:
        target = {"matcher": "", "hooks": []}
        entries.append(target)

    target_hooks = target.setdefault("hooks", [])
    if not any(h.get("command") == cmd for h in target_hooks):
        target_hooks.append({"type": "command", "command": cmd})
        added += 1

settings_path.write_text(json.dumps(data, indent=2) + "\n")
print(f"Added {added} hook entries across {len(events)} events.")
PY

echo "Done. Restart Claude Code to pick up the new hooks."
