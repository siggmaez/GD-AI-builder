"""ChatGPT account login through the official Codex CLI; never expose credentials."""
import os
import re
import subprocess
import threading
import time


def account_env():
    env = os.environ.copy()
    for name in ("OPENAI_API_KEY", "CODEX_API_KEY", "OPENAI_BASE_URL", "CODEX_ACCESS_TOKEN"):
        env.pop(name, None)
    return env


def has_chatgpt_login(command):
    result = subprocess.run(command("codex", "login", "status"), capture_output=True,
                            text=True, encoding="utf-8", errors="replace", timeout=10,
                            env=account_env())
    return result.returncode == 0 and "chatgpt" in (result.stdout + result.stderr).lower()


class AccountLogin:
    def __init__(self):
        self.lock = threading.Lock()
        self.state = {"state": "idle", "url": "", "code": "", "message": "Sign in with your ChatGPT account."}

    def snapshot(self):
        with self.lock:
            return dict(self.state)

    def update(self, **fields):
        with self.lock:
            self.state.update(fields)

    def start(self, command):
        with self.lock:
            if self.state["state"] in ("starting", "waiting"):
                return dict(self.state)
            self.state = {"state": "starting", "url": "", "code": "", "message": "Starting official Codex sign-in..."}
        threading.Thread(target=self.worker, args=(command,), daemon=True).start()
        return self.snapshot()

    def worker(self, command):
        process = None
        try:
            if has_chatgpt_login(command):
                return self.update(state="signed_in", message="Signed in with ChatGPT. Return to Geometry Dash.")
            process = subprocess.Popen(command("codex", "login", "--device-auth"),
                                       stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                       text=True, encoding="utf-8", errors="replace", env=account_env())
            timer = threading.Timer(600, process.kill)
            timer.daemon = True
            timer.start()
            try:
                for line in process.stdout:
                    clean = re.sub(r"\x1b\[[0-?]*[ -/]*[@-~]", "", line)
                    url = re.search(r"https://auth\.openai\.com/[A-Za-z0-9/_?=&.%-]+", clean)
                    code = re.search(r"\b[A-Z0-9]{4,6}-[A-Z0-9]{4,6}\b", clean)
                    fields = {"state": "waiting", "message": "Open the official page and enter this code."}
                    if url:
                        fields["url"] = url.group(0)
                    if code:
                        fields["code"] = code.group(0)
                    # Only publish an official URL and a device code, never raw CLI output.
                    if url or code:
                        self.update(**fields)
                process.wait()
            finally:
                timer.cancel()
                process.stdout.close()
            if has_chatgpt_login(command):
                self.update(state="signed_in", code="", message="Signed in with ChatGPT. Return to Geometry Dash.")
            else:
                self.update(state="error", code="", message="Sign-in did not finish. Enable device-code login in ChatGPT security settings and retry.")
        except Exception:
            if process and process.poll() is None:
                process.kill()
            self.update(state="error", code="", message="Cannot start Codex sign-in. Check that Codex CLI is installed on the bridge device.")
