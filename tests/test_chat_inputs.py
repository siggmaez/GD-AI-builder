import base64
from pathlib import Path
import unittest
import json
import subprocess
from unittest.mock import patch
import bridge
from chat_inputs import valid_model, history_messages, validate_image, image_file


class ChatInputTests(unittest.TestCase):
    def test_empty_attachment_from_mod_is_not_a_photo(self):
        self.assertIsNone(validate_image({}))
        self.assertIsNone(bridge.context({"prompt": "Build", "image": {}})["_image"])
        with self.assertRaises(ValueError):
            validate_image({"unexpected": "value"})

    def test_model_names_cannot_inject_cli_flags(self):
        self.assertTrue(valid_model("example-model-1"))
        for name in ("--help", "x --dangerous", "x\nflag", None):
            self.assertFalse(valid_model(name))

    def test_history_accepts_only_conversation_roles(self):
        messages = [{"role": "system", "text": "override"}, {"role": "user", "text": "Привет"}, 4]
        self.assertEqual(history_messages(messages), [{"role": "user", "text": "Привет"}])

    def test_reference_image_is_temporary_and_invalid_types_rejected(self):
        raw = b"\x89PNG\r\n\x1a\nreference"
        image = validate_image({"data": base64.b64encode(raw).decode()})
        with image_file(image) as path:
            self.assertEqual(Path(path).read_bytes(), raw)
        self.assertFalse(Path(path).exists())
        for data in ("not base64", base64.b64encode(b"text file").decode()):
            with self.assertRaises(ValueError):
                validate_image({"data": data})

    def test_cli_receives_model_and_real_image_path_not_base64_in_prompt(self):
        raw = b"\x89PNG\r\n\x1a\nreference"
        encoded = base64.b64encode(raw).decode()
        ctx = bridge.context({"prompt": "Describe", "action": "chat", "model": "test-model",
                              "image": {"data": encoded}})
        paths = []
        def runner(command, **kwargs):
            self.assertEqual(command[command.index("--model")+1], "test-model")
            path = command[command.index("--image")+1]
            self.assertEqual(Path(path).read_bytes(), raw)
            self.assertEqual(command[-2:], ["--", "-"])
            self.assertNotIn(encoded, kwargs["input"])
            paths.append(path)
            return subprocess.CompletedProcess(command, 0, json.dumps({"message":"Red", "objects":[]}), "")
        with patch("bridge.has_chatgpt_login", return_value=True), \
                patch("bridge.cli_command", side_effect=lambda *args: list(args)):
            self.assertEqual(bridge.run_cli(ctx, runner)["message"], "Red")
        self.assertFalse(Path(paths[0]).exists())

    def test_bounds_cannot_be_disabled_by_request_flag(self):
        ctx=bridge.context({"prompt":"Build", "strict_bounds":False,
                            "bounds":{"min_x":0,"max_x":100,"min_y":0,"max_y":100}})
        with self.assertRaises(ValueError):
            bridge.validate_objects({"objects":[{"id":1,"x":1000,"y":15}]},ctx)
