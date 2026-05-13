#!/bin/bash
# Claudy hook bridge. Called by Claude Code with hook JSON on stdin.
# We forward the work to send_state.py in the background so the hook
# never blocks Claude Code.

INPUT=$(cat)
HERE="$(cd "$(dirname "$0")" && pwd)"

(printf '%s' "$INPUT" | /usr/bin/env python3 "$HERE/send_state.py" >/dev/null 2>&1) &
disown
exit 0
