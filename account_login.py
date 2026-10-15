"""ChatGPT account login through the official Codex CLI; never expose credentials."""
import os
import re
import subprocess
import threading
import time

DEVICE_URL = "https://auth.openai.com/codex/device"


def login_error(output, timed_out=False):
    """Classify failures without forwarding CLI output or credentials to the UI."""
    text = output.lower()
    if timed_out:
        return "OpenAI did not return a login code within 45 seconds. Check your internet connection and retry."
    if any(word in text for word in ("certificate", "ssl", "tls")):
        return "Cannot verify the secure connection to OpenAI. Check the phone date and network; update the bridge app."
    if any(word in text for word in ("403", "forbidden", "access denied")):
        return "OpenAI refused the login request (403). Try another network and check account access."
    if any(word in text for word in ("resolve", "dns", "connect", "network", "timed out")):
        return "Cannot reach OpenAI. Check your internet connection and retry."
    if "not enabled" in text:
        return "Device-code sign-in is unavailable. Check device-code login in ChatGPT security settings."
    return "Sign-in did not finish. Retry; check device-code login in ChatGPT security settings."


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
            self.state = {"state": "starting", "url": DEVICE_URL, "code": "", "message": "Requesting a login code from OpenAI (up to 45 seconds)..."}
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
            output = []
            timed_out = threading.Event()
            code_received = threading.Event()
            def startup_timeout():
                if not code_received.is_set() and process.poll() is None:
                    timed_out.set()
                    process.kill()
            startup_timer = threading.Timer(45, startup_timeout)
            startup_timer.daemon = True
            startup_timer.start()
            timer = threading.Timer(960, process.kill)
            timer.daemon = True
            timer.start()
            try:
                for line in process.stdout:
                    clean = re.sub(r"\x1b\[[0-?]*[ -/]*[@-~]", "", line)
                    output.append(clean[:2048])
                    output = output[-16:]
                    url = re.search(r"https://auth\.openai\.com/[A-Za-z0-9/_?=&.%-]+", clean)
                    code = re.search(r"\b[A-Z0-9]{4,6}-[A-Z0-9]{4,6}\b", clean)
                    fields = {"state": "waiting", "message": "Open the official page and enter this code."}
                    if url:
                        fields["url"] = url.group(0)
                    if code:
                        fields["code"] = code.group(0)
                        code_received.set()
                        startup_timer.cancel()
                    # Only publish an official URL and a device code, never raw CLI output.
                    if url or code:
                        self.update(**fields)
                process.wait()
            finally:
                timer.cancel()
                startup_timer.cancel()
                process.stdout.close()
            if has_chatgpt_login(command):
                self.update(state="signed_in", code="", message="Signed in with ChatGPT. Return to Geometry Dash.")
            else:
                self.update(state="error", code="", message=login_error("".join(output), timed_out.is_set()))
        except Exception:
            if process and process.poll() is None:
                process.kill()
            self.update(state="error", code="", message="Cannot start Codex sign-in. Check that Codex CLI is installed on the bridge device.")
