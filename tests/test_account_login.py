import io
import os
import unittest
from unittest.mock import patch, Mock
import account_login


class AccountTests(unittest.TestCase):
    def test_api_keys_removed_from_child_environment(self):
        with patch.dict(os.environ, {"OPENAI_API_KEY": "fake-test-key", "CODEX_API_KEY": "fake", "OPENAI_BASE_URL": "https://example.invalid"}):
            child = account_login.account_env()
            self.assertNotIn("OPENAI_API_KEY", child)
            self.assertNotIn("CODEX_API_KEY", child)
            self.assertNotIn("OPENAI_BASE_URL", child)

    def test_status_accepts_only_chatgpt(self):
        for text, expected in (("Logged in using ChatGPT", True), ("Logged in using an API key", False)):
            with patch("account_login.subprocess.run", return_value=Mock(returncode=0, stdout=text, stderr="")):
                self.assertEqual(account_login.has_chatgpt_login(lambda *x: list(x)), expected)

    def test_device_login_does_not_expose_raw_cli_output(self):
        login = account_login.AccountLogin()
        process = Mock(stdout=io.StringIO("https://auth.openai.com/codex/device\nABCD-EFGHI\nsecret-test-token\n"))
        with patch("account_login.has_chatgpt_login", side_effect=[False, True]), patch("account_login.subprocess.Popen", return_value=process):
            login.worker(lambda *args: list(args))
        state = login.snapshot()
        self.assertEqual(state["state"], "signed_in")
        self.assertEqual(state["code"], "")
        self.assertNotIn("secret-test-token", str(state))

    def test_cached_account_skips_new_login(self):
        login = account_login.AccountLogin()
        with patch("account_login.has_chatgpt_login", return_value=True), patch("account_login.subprocess.Popen") as spawn:
            login.worker(lambda *x: list(x))
            spawn.assert_not_called()
        self.assertEqual(login.snapshot()["state"], "signed_in")
