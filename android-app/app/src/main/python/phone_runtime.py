"""App-private native Codex + the same tested bridge. No Termux and no API keys."""
import importlib
import os
from pathlib import Path
import subprocess
import sys
import threading
from http.server import ThreadingHTTPServer

_lock = threading.RLock()
_server = None
_bridge = None
_error = ''
_version = ''


def start(native_directory, private_directory, cache_directory):
    global _server, _bridge, _error, _version
    with _lock:
        if _server is not None:
            return status()
        private = Path(private_directory)
        executable = Path(native_directory) / 'libcodex.so'
        if not executable.is_file():
            raise RuntimeError('Native Codex runtime is missing. Install the complete arm64 APK.')
        os.environ.update(HOME=str(private), CODEX_HOME=str(private / '.codex'),
                          TMPDIR=str(cache_directory), SHELL='/system/bin/sh',
                          SSL_CERT_FILE=str(private / 'system-ca.pem'),
                          AI_BUILDER_CODEX_BINARY=str(executable), BRIDGE_HOST='127.0.0.1', BRIDGE_PORT='8765')
        for key in ('OPENAI_API_KEY', 'CODEX_API_KEY', 'CODEX_ACCESS_TOKEN', 'OPENAI_BASE_URL'):
            os.environ.pop(key, None)
        working = private / 'workspace'
        working.mkdir(exist_ok=True)
        configuration = private / '.codex'
        configuration.mkdir(exist_ok=True)
        # The phone companion generates JSON, not shell commands. Android's
        # app sandbox also isolates its private files from Geometry Dash.
        (configuration / 'config.toml').write_text('[features]\nshell_tool = false\n', encoding='utf-8')
        os.chdir(working)
        bridge_path = str(private / 'bridge')
        if bridge_path not in sys.path:
            sys.path.insert(0, bridge_path)
        _error = ''
        try:
            version = subprocess.run([str(executable), '--version'], capture_output=True,
                                     encoding='utf-8', errors='replace', timeout=15)
            if version.returncode:
                raise RuntimeError('The native Codex runtime could not start on this Android device.')
            _version = version.stdout.strip()[:80]
            _bridge = importlib.import_module('bridge')
            server = ThreadingHTTPServer(('127.0.0.1', 8765), _bridge.Handler)
            server.daemon_threads = True
            _server = server
            threading.Thread(target=server.serve_forever, name='ai-builder-http', daemon=True).start()
        except Exception as exc:
            _error = str(exc)
            raise
        return status()


def status():
    import json
    with _lock:
        return json.dumps({'running': _server is not None, 'runtime': _version, 'error': _error,
                           'account': _bridge.ACCOUNT.snapshot() if _bridge else {'state': 'idle', 'url': '', 'code': ''}})


def sign_in():
    with _lock:
        if _server is None:
            raise RuntimeError('Start the bridge first.')
        _bridge.ACCOUNT.start(_bridge.cli_command)


def stop():
    global _server
    with _lock:
        server, _server = _server, None
    if server:
        server.shutdown()
        server.server_close()
