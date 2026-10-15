import base64
from pathlib import Path
import unittest
from chat_inputs import valid_model, history_messages, validate_image, image_file


class ChatInputTests(unittest.TestCase):
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
