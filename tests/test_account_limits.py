import unittest
from unittest.mock import patch
import account_limits


class LimitTests(unittest.TestCase):
    def test_usage_is_converted_to_remaining_and_keeps_reset_time(self):
        result = account_limits.summarize({"rateLimits": {
            "primary": {"usedPercent": 32, "windowDurationMins": 300, "resetsAt": 123456},
            "secondary": {"usedPercent": 5, "windowDurationMins": 10080}}})
        self.assertTrue(result["available"])
        self.assertEqual([w["remaining_percent"] for w in result["windows"]], [68, 95])
        self.assertEqual(result["windows"][0]["resets_at"], 123456)

    def test_missing_malformed_or_nonfinite_data_never_fakes_a_quota(self):
        for value in (None, {}, {"rateLimits": None}, {"rateLimits": {
            "primary": {"usedPercent": float("nan")}, "secondary": {"usedPercent": True}}}):
            self.assertFalse(account_limits.summarize(value)["available"])

    def test_api_key_login_cannot_start_account_server(self):
        with patch("account_limits.has_chatgpt_login", return_value=False), \
                patch("account_limits.subprocess.Popen") as process:
            self.assertFalse(account_limits.read_limits(lambda *args: list(args))["available"])
            process.assert_not_called()

    def test_cached_limits_do_not_poll_on_each_ui_refresh(self):
        with patch.object(account_limits, "_updated", 0), \
                patch("account_limits.time.monotonic", return_value=100), \
                patch("account_limits.read_limits", return_value={"available": False, "windows": []}) as read:
            account_limits.cached_limits(None)
            account_limits.cached_limits(None)
            read.assert_called_once()
