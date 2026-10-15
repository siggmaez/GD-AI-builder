#!/usr/bin/env python3
"""Local bridge: GD mod -> CLI agent (uses your logged-in subscription).

Run in Termux on the phone, or on a PC in the same Wi-Fi:
    python bridge.py            # default CLI: claude
    BRIDGE_CLI=codex python bridge.py
Mod setting "Bridge URL": http://127.0.0.1:8765 (phone) or http://<pc-ip>:8765
"""
import json, os, re, subprocess
from http.server import BaseHTTPRequestHandler, HTTPServer

CLI = os.environ.get("BRIDGE_CLI", "claude")
PORT = int(os.environ.get("BRIDGE_PORT", "8765"))

COMMANDS = {
    "claude": lambda p: ["claude", "-p", p],
    "codex": lambda p: ["codex", "exec", p],
    "gemini": lambda p: ["gemini", "-p", p],
}

SYSTEM = """You generate Geometry Dash level objects. Reply with ONLY JSON:
{"objects":[{"id":int,"x":float,"y":float,"rot":float,"scale":float}]}
Rules: 1 block = 30 units. Ground y = 15. Use only common object IDs:
1 = block, 8 = spike, 35 = yellow pad, 36 = yellow orb, 67 = blue pad,
(decor) 1 = block, 8 = spike, 1007+ = decoration pieces.
Start placing at x = start_x + 60. Keep jumps beatable: gap <= 4 blocks at cube speed.
Mode: {mode}. Request: {prompt}
"""

class H(BaseHTTPRequestHandler):
    def _send(self, code, obj):
        data = json.dumps(obj).encode()
        self.send_response(code)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def do_POST(self):
        if self.path != "/generate":
            return self._send(404, {"error": "not found"})
        body = json.loads(self.rfile.read(int(self.headers.get("Content-Length", 0))) or b"{}")
        prompt = SYSTEM.replace("{mode}", str(body.get("mode", "gameplay"))) \
                       .replace("{prompt}", str(body.get("prompt", ""))) \
                       .replace("start_x", str(body.get("start_x", 0)))
        try:
            out = subprocess.run(COMMANDS[CLI](prompt), capture_output=True, text=True, timeout=170).stdout
            m = re.search(r"\{.*\}", out, re.S)
            data = json.loads(m.group(0)) if m else {"objects": []}
            if not isinstance(data.get("objects"), list):
                data = {"objects": []}
            # cap size to avoid freezing the editor
            data["objects"] = data["objects"][:1500]
            self._send(200, data)
        except Exception as e:
            self._send(500, {"error": str(e)})

    def log_message(self, *a):
        pass

if __name__ == "__main__":
    print(f"Bridge on :{PORT}, CLI = {CLI}")
    HTTPServer(("0.0.0.0", PORT), H).serve_forever()
