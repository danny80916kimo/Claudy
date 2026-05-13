#!/usr/bin/env python3
"""
Claudy bridge: maps a Claude Code hook event (JSON on stdin) to an ESP32
state update and POSTs it to the device.

Configure via environment variables (set in ~/.zshrc or per-hook):
  CLAUDY_URL    full URL of /state endpoint (default: http://claudy.local/state)
  CLAUDY_TOKEN  optional shared secret matching AUTH_TOKEN in firmware/config.h
  CLAUDY_MAX_TOKENS  context budget used for the progress bar (default 200000)
"""
import json
import os
import re
import subprocess
import sys
import time
import urllib.request
import urllib.error
from urllib.parse import urlparse, urlunparse

CLAUDY_URL = os.environ.get("CLAUDY_URL", "http://claudy.local/state")
AUTH_TOKEN = os.environ.get("CLAUDY_TOKEN", "")
# CLAUDY_MAX_TOKENS overrides the auto-detected context window if set.
MAX_TOKENS_OVERRIDE = int(os.environ.get("CLAUDY_MAX_TOKENS", "0"))
IP_CACHE   = os.path.expanduser("~/.claude/claudy_ip_cache")
IP_CACHE_TTL = 60  # seconds


def _swap_host(url, ip):
    p = urlparse(url)
    netloc = f"{ip}:{p.port}" if p.port else ip
    return urlunparse(p._replace(netloc=netloc))


def _resolve_mdns(hostname, timeout=2.0):
    """Resolve a .local name via macOS dns-sd. Returns IPv4 string or None."""
    try:
        proc = subprocess.Popen(
            ["dns-sd", "-G", "v4", hostname],
            stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, text=True
        )
    except Exception:
        return None
    ipv4 = re.compile(r"\b(\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3})\b")
    deadline = time.time() + timeout
    try:
        while time.time() < deadline:
            line = proc.stdout.readline()
            if not line:
                if proc.poll() is not None:
                    break
                continue
            if " Add " in line:
                m = ipv4.search(line)
                if m:
                    return m.group(1)
    finally:
        try: proc.terminate()
        except Exception: pass
    return None


def _resolve_url_with_cache(url):
    """For .local URLs, pre-resolve via dns-sd with a small file cache so we
    don't re-query every hook (and don't depend on flaky system mDNS)."""
    p = urlparse(url)
    host = p.hostname or ""
    if not host.endswith(".local"):
        return url
    try:
        if os.path.exists(IP_CACHE) and time.time() - os.path.getmtime(IP_CACHE) < IP_CACHE_TTL:
            ip = open(IP_CACHE).read().strip()
            if ip:
                return _swap_host(url, ip)
    except Exception:
        pass
    ip = _resolve_mdns(host)
    if not ip:
        return url
    try:
        os.makedirs(os.path.dirname(IP_CACHE), exist_ok=True)
        with open(IP_CACHE, "w") as f:
            f.write(ip)
    except Exception:
        pass
    return _swap_host(url, ip)


def post(payload):
    url = _resolve_url_with_cache(CLAUDY_URL)
    data = json.dumps(payload).encode()
    req = urllib.request.Request(url, data=data, method="POST")
    req.add_header("Content-Type", "application/json")
    if AUTH_TOKEN:
        req.add_header("X-Claudy-Token", AUTH_TOKEN)
    try:
        urllib.request.urlopen(req, timeout=2.0).read()
    except Exception:
        # On failure, invalidate cache so next call re-resolves.
        try: os.remove(IP_CACHE)
        except Exception: pass


def _context_window_for_model(model, observed_used=0):
    """Map Claude model ID + observed usage to a context-window size.

    Claude Code transcripts don't record the `[1m]` suffix even when the
    1M-context variant is in use, so we also infer from observed usage:
    if the recorded `used` is already over the standard 200K window, the
    session must be on the 1M variant."""
    if observed_used > 200000:
        return 1000000
    if model and "[1m]" in model.lower():
        return 1000000
    return 200000


def tokens_from_transcript(path):
    """Read the last `usage` block from a JSONL transcript and return
    (context_tokens_used, context_window) for the most recent assistant turn,
    or (None, None) if not found.

    `context_tokens_used` = input + cache_read + cache_creation
    (i.e., what the model just processed as input context — matches what
    Claude Code itself displays). We deliberately omit output_tokens because
    those are the *response*, not yet part of the context window."""
    if not path or not os.path.exists(path):
        return (None, None)
    try:
        size = os.path.getsize(path)
        with open(path, "rb") as f:
            if size > 65536:
                f.seek(size - 65536)
                f.readline()  # drop partial first line
            tail = f.read().decode("utf-8", errors="ignore").splitlines()
        for line in reversed(tail):
            try:
                obj = json.loads(line)
            except Exception:
                continue
            msg = obj.get("message") or {}
            usage = msg.get("usage") or obj.get("usage") or {}
            if not usage:
                continue
            used = (
                usage.get("input_tokens", 0)
                + usage.get("cache_read_input_tokens", 0)
                + usage.get("cache_creation_input_tokens", 0)
            )
            if used <= 0:
                continue
            model = msg.get("model") or obj.get("model") or ""
            window = _context_window_for_model(model, used)
            return (used, window)
    except Exception:
        return (None, None)
    return (None, None)


def brief(s, n=58):
    if s is None:
        return ""
    s = str(s).replace("\n", " ").strip()
    return (s[: n - 1] + "…") if len(s) > n else s


def map_event(ev):
    """Return (state, message) for an event, or (None, None) to skip."""
    name = ev.get("hook_event_name", "")
    tool = ev.get("tool_name", "")
    inp = ev.get("tool_input") or {}

    if name == "SessionStart":
        return "idle", "Session started"
    if name == "UserPromptSubmit":
        return "thinking", brief(ev.get("prompt", ""))
    if name == "PreToolUse":
        if tool == "Bash":
            return "working", brief(inp.get("command", ""))
        if tool == "Read":
            return "working", brief(os.path.basename(inp.get("file_path", "")))
        if tool in ("Edit", "MultiEdit", "Write"):
            return "working", brief(os.path.basename(inp.get("file_path", "")))
        if tool in ("Grep", "Glob"):
            return "working", brief(inp.get("pattern") or inp.get("query", ""))
        if tool in ("WebFetch", "WebSearch"):
            return "working", brief(inp.get("url") or inp.get("query", ""))
        if tool in ("Task", "Agent"):
            return "working", brief(inp.get("description") or inp.get("prompt", ""))
        return "working", brief(tool)
    if name == "PostToolUse":
        return "thinking", brief(tool)
    if name == "PostToolUseFailure":
        return "error", brief(f"{tool} failed")
    if name in ("Notification", "PermissionRequest", "Elicitation"):
        return "waiting", brief(ev.get("message") or f"Permission: {tool}")
    if name in ("Stop", "TaskCompleted"):
        return "done", "Done"
    if name in ("StopFailure", "PermissionDenied"):
        return "error", brief(name)
    if name in ("SessionEnd",):
        return "idle", "Idle"
    return None, None


def main():
    try:
        ev = json.load(sys.stdin)
    except Exception:
        return

    state, msg = map_event(ev)
    if state is None:
        return

    payload = {
        "state": state,
        "tool": ev.get("tool_name", ""),
        "message": msg,
        "event": ev.get("hook_event_name", ""),
    }

    used, window = tokens_from_transcript(ev.get("transcript_path"))
    if used is not None:
        max_tokens = MAX_TOKENS_OVERRIDE if MAX_TOKENS_OVERRIDE > 0 else window
        payload["tokens"] = {"used": used, "max": max_tokens}

    post(payload)

    if state == "done":
        time.sleep(3)
        post({"state": "idle", "message": "Idle"})


if __name__ == "__main__":
    main()
