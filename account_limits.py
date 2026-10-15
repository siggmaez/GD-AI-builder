"""Read subscription limits through the official local Codex app-server protocol."""
import json
import math
import queue
import subprocess
import threading
import time
from account_login import account_env, has_chatgpt_login

_lock = threading.Lock()
_cache = {"available": False, "windows": [], "message": "Limits unavailable"}
_updated = 0.


def summarize(result):
    if not isinstance(result, dict):
        result = {}
    data = result.get("rateLimits", {})
    if not isinstance(data, dict):
        data = {}
    windows = []
    for name in ("primary", "secondary"):
        item = data.get(name)
        if not isinstance(item, dict):
            continue
        used = item.get("usedPercent")
        if type(used) not in (int, float) or not math.isfinite(used):
            continue
        windows.append({"name": name, "remaining_percent": round(max(0, min(100, 100-used)), 1),
                        "window_minutes": item.get("windowDurationMins"), "resets_at": item.get("resetsAt")})
    return {"available": bool(windows), "windows": windows,
            "message": "Account limits" if windows else "Limits unavailable"}


def read_limits(command):
    process = None
    try:
        if not has_chatgpt_login(command):
            return {"available": False, "windows": [], "message": "Sign in with ChatGPT"}
        process = subprocess.Popen(command("codex", "app-server", "--stdio"),
                                   stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL,
                                   text=True, encoding="utf-8", errors="replace", env=account_env())
        messages = queue.Queue()
        def reader():
            try:
                for line in process.stdout:
                    try: messages.put(json.loads(line))
                    except ValueError: pass
            except (OSError, ValueError):
                pass
        thread = threading.Thread(target=reader, daemon=True)
        thread.start()
        def send(data):
            process.stdin.write(json.dumps(data)+"\n"); process.stdin.flush()
        send({"id": 1, "method": "initialize", "params": {"clientInfo": {"name": "gd_ai_builder", "version": "0.5.0"}}})
        deadline = time.monotonic()+12
        while time.monotonic() < deadline:
            m = messages.get(timeout=max(.1, deadline-time.monotonic()))
            if m.get("id") == 1:
                if "error" in m: break
                send({"method": "initialized", "params": {}})
                send({"id": 2, "method": "account/rateLimits/read", "params": {}})
            elif m.get("id") == 2:
                return summarize(m.get("result", {}))
    except Exception:
        pass
    finally:
        if process:
            if process.poll() is None: process.terminate()
            try: process.wait(timeout=2)
            except subprocess.TimeoutExpired: process.kill(); process.wait()
            if process.stdin: process.stdin.close()
            if process.stdout: process.stdout.close()
    return {"available": False, "windows": [], "message": "Limits unavailable"}


def cached_limits(command):
    global _cache, _updated
    with _lock:
        if time.monotonic()-_updated > 60:
            _cache = read_limits(command)
            _updated = time.monotonic()
        return _cache
