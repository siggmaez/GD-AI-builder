#!/data/data/com.termux/files/usr/bin/bash
# Install as ~/.shortcuts/AI-Builder (Termux:Widget), or run once from Termux.
set -eu
project_dir="${AI_BUILDER_HOME:-$HOME/GD-AI-builder}"
cd "$project_dir"
mkdir -p "$HOME/.cache/ai-builder"
if curl -fsS http://127.0.0.1:8765/health >/dev/null 2>&1; then
    termux-open-url http://127.0.0.1:8765/login
    exit 0
fi
command -v termux-wake-lock >/dev/null && termux-wake-lock || true
nohup python bridge.py >"$HOME/.cache/ai-builder/bridge.log" 2>&1 &
for attempt in 1 2 3 4 5; do
    if curl -fsS http://127.0.0.1:8765/health >/dev/null 2>&1; then
        termux-open-url http://127.0.0.1:8765/login
        exit 0
    fi
    sleep 1
done
printf 'Bridge did not start. See ~/.cache/ai-builder/bridge.log\n'
