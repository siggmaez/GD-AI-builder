import json
import subprocess
import threading
import unittest
import urllib.error
import urllib.request
from unittest.mock import patch
import bridge


class BridgeTests(unittest.TestCase):
    def setUp(self):
        login = patch("bridge.has_chatgpt_login", return_value=True)
        login.start()
        self.addCleanup(login.stop)
    def ctx(self, **kwargs):
        return bridge.context({"prompt": "short jump", **kwargs})

    def test_json_fences_prose_nested_and_braces_in_strings(self):
        data = {"objects": [{"id": 1, "x": 75, "y": 15}], "note": "brace } {"}
        for text in (json.dumps(data), "Text\n```json\n" + json.dumps(data) + "\n``` done",
                     '{} log\n{"meta": 1}\n' + json.dumps(data)):
            self.assertEqual(bridge.extract_json(text), data)

    def test_reject_invalid_json_and_wrong_schema(self):
        for text in ('{"objects":}', '{"objects":{}}', 'no JSON', '{"objects":[1]'):
            with self.assertRaises(ValueError):
                bridge.extract_json(text)

    def test_validation_drops_junk_and_limits_valid_objects(self):
        valid = {"id": 1, "x": 75, "y": 15}
        junk = [None, 42, {"id": 999999, "x": 75, "y": 15},
                {"id": True, "x": 75, "y": 15}, {"id": 1},
                {**valid, "x": float("nan")}, {**valid, "y": -1},
                {**valid, "scale": 100}, {**valid, "rot": "nan"}]
        out = bridge.validate_objects({"objects": junk + [{**valid, "x":75+i*30} for i in range(5)]}, self.ctx(max_objects=2))
        self.assertEqual(len(out["objects"]), 2)

    def test_decor_forbids_gameplay_ids(self):
        ctx = self.ctx(mode="decor")
        with self.assertRaises(ValueError):
            bridge.validate_objects({"objects": [{"id": 8, "x": 75, "y": 15}]}, ctx)

    def test_triple_spikes_and_portals_are_allowed(self):
        objects = [{"id": 8, "x": x, "y": 15} for x in (75, 105, 135)]
        objects += [{"id": 13, "x": 315, "y": 150}, {"id": 660, "x": 615, "y": 150}]
        result = bridge.validate_objects({"objects": objects}, self.ctx())
        self.assertEqual([o["id"] for o in result["objects"]], [8,8,8,13,660])

    def test_unlimited_and_large_limits_are_not_capped_at_1500(self):
        objects = [{"id": 83, "x": 15 + i / 10, "y": 15} for i in range(2001)]
        for limit in (0, 100000):
            result = bridge.validate_objects({"objects": objects}, self.ctx(max_objects=limit))
            self.assertEqual(len(result["objects"]), 2001)

    def test_duplicates_dropped_and_mode_toggle_honored(self):
        spike = {"id": 8, "x": 75, "y": 15}
        portal = {"id": 13, "x": 315, "y": 150}
        result = bridge.validate_objects({"objects": [spike, spike, portal]}, self.ctx(allow_mode_changes=False))
        self.assertEqual(len(result["objects"]), 1)

    def test_codex_api_key_login_is_rejected(self):
        with patch("bridge.has_chatgpt_login", return_value=False):
            with self.assertRaises(bridge.BridgeError) as err:
                bridge.run_cli(self.ctx())
            self.assertEqual(err.exception.code, "cli_not_logged_in")

    def test_context_rejects_bad_inputs(self):
        for body in ([], {}, {"prompt": "x", "cli": "shell"}, {"prompt": "x", "start_x": float("inf")},
                     {"prompt": "x", "bounds": {}}, {"prompt": "x", "max_objects": True},
                     {"prompt": "x", "cli": []}, {"prompt": "x", "start_x": 10 ** 1000}):
            with self.assertRaises(bridge.BridgeError):
                bridge.context(body)

    def test_chat_returns_reply_and_never_objects(self):
        ctx = self.ctx(action="chat")
        result = bridge.validate_objects({"message": "Привет!\nЧто построим?", "objects": [
            {"id": 1, "x": 75, "y": 15}]}, ctx)
        self.assertEqual(result, {"objects": [], "message": "Привет! Что построим?"})
        with self.assertRaises(ValueError):
            bridge.validate_objects({"objects": []}, ctx)

    def test_russian_context_roundtrip_and_character_limit(self):
        prompt = 'Ёжик, сделай лёгкий геймплей — «прыжок» №1!'
        ctx = self.ctx(prompt=prompt, language="ru")
        self.assertEqual(json.loads(json.dumps(ctx, ensure_ascii=False))["prompt"], prompt)
        self.assertEqual(ctx["language"], "ru")
        self.assertEqual(len(self.ctx(prompt="ё" * 300)["prompt"]), 300)
        with self.assertRaises(bridge.BridgeError):
            self.ctx(prompt="ё" * 301)

    def test_bundled_font_has_russian_alphabet_and_valid_atlas_rects(self):
        from pathlib import Path
        import re
        font = (Path(bridge.__file__).parent / "resources/ai-builder-font.fnt").read_text(encoding="utf-8")
        glyphs = {}
        for line in font.splitlines():
            if line.startswith("char "):
                values = {key: int(value) for key, value in re.findall(r"(\w+)=(-?\d+)", line)}
                glyphs[values["id"]] = values
        for char in "АБВГДЕЁЖЗИЙКЛМНОПРСТУФХЦЧШЩЪЫЬЭЮЯабвгдеёжзийклмнопрстуфхцчшщъыьэюя«»—№":
            self.assertIn(ord(char), glyphs)
        for v in glyphs.values():
            self.assertTrue(0 <= v["x"] <= v["x"] + v["width"] <= 1024)
            self.assertTrue(0 <= v["y"] <= v["y"] + v["height"] <= 2048)

    def test_build_preserves_short_ai_message(self):
        result = bridge.validate_objects({"objects": [], "message": "a" * 1000}, self.ctx())
        self.assertEqual(len(result["message"]), 220)

    def test_disconnected_windows_client_does_not_crash_handler(self):
        handler = object.__new__(bridge.Handler)
        handler.send_response = lambda *args: None
        handler.send_header = lambda *args: None
        handler.end_headers = lambda: None
        class Disconnected:
            def write(self, data):
                raise ConnectionAbortedError("client cancelled")
        handler.wfile = Disconnected()
        with patch("builtins.print"):
            handler.send_json(200, {"objects": []})

    @patch("bridge.cli_command", return_value=["fake", "prompt"])
    def test_exactly_one_retry(self, _):
        responses = iter(["not JSON", '{"objects":[]}'])
        calls = []
        def run(cmd, **kwargs):
            calls.append(list(cmd))
            return subprocess.CompletedProcess(cmd, 0, next(responses), "")
        self.assertEqual(bridge.run_cli(self.ctx(), run), {"objects": []})
        self.assertEqual(len(calls), 2)
        self.assertIn("Previous answer invalid", calls[1][-1])
        def bad(cmd, **kwargs):
            return subprocess.CompletedProcess(cmd, 0, "bad", "")
        with self.assertRaises(bridge.BridgeError) as err:
            bridge.run_cli(self.ctx(), bad)
        self.assertEqual(err.exception.code, "invalid_json")

    @patch("bridge.cli_command", return_value=["fake", "prompt"])
    def test_auth_and_timeout_errors(self, _):
        def auth(cmd, **kwargs):
            return subprocess.CompletedProcess(cmd, 1, "", "Please login first")
        with self.assertRaises(bridge.BridgeError) as err:
            bridge.run_cli(self.ctx(), auth)
        self.assertEqual(err.exception.code, "cli_not_logged_in")
        def timeout(cmd, **kwargs):
            raise subprocess.TimeoutExpired(cmd, 1)
        with self.assertRaises(bridge.BridgeError) as err:
            bridge.run_cli(self.ctx(), timeout)
        self.assertEqual(err.exception.code, "timeout")

    def test_http_health_bad_body_and_generation(self):
        server = bridge.ThreadingHTTPServer(("127.0.0.1", 0), bridge.Handler)
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        url = "http://127.0.0.1:" + str(server.server_port)
        try:
            with urllib.request.urlopen(url + "/health") as r:
                self.assertEqual(json.load(r)["status"], "ok")
            with self.assertRaises(urllib.error.HTTPError) as e:
                urllib.request.urlopen(urllib.request.Request(url + "/generate", data=b"bad"))
            self.assertEqual(e.exception.code, 400)
            e.exception.close()
            with patch("bridge.run_cli", return_value={"objects": []}):
                req = urllib.request.Request(url + "/generate", data=json.dumps({"prompt": "x"}).encode())
                with urllib.request.urlopen(req) as r:
                    self.assertEqual(json.load(r), {"objects": []})
        finally:
            server.shutdown()
            server.server_close()
            thread.join()


if __name__ == "__main__":
    unittest.main()
