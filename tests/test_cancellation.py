import json
import subprocess
import sys
import threading
import time
import unittest
import urllib.request
import urllib.error
import bridge


class CancellationTests(unittest.TestCase):
    def test_process_pipes_survive_polling_and_preserve_stdin(self):
        result=bridge.cancellable_run([sys.executable,"-c",
            "import sys,time; text=sys.stdin.read(); time.sleep(.5); print(text)"],
            cancel=threading.Event(),timeout=5,text=True,encoding="utf-8",input="test prompt")
        self.assertEqual(result.stdout.strip(),"test prompt")

    def test_stop_terminates_a_running_process(self):
        event=threading.Event()
        timer=threading.Timer(.3,event.set)
        timer.start()
        started=time.monotonic()
        try:
            with self.assertRaises(bridge.BridgeError) as error:
                bridge.cancellable_run([sys.executable,"-c","import time; time.sleep(30)"],
                    cancel=event,timeout=10,text=True,encoding="utf-8")
            self.assertEqual(error.exception.code,"cancelled")
            self.assertLess(time.monotonic()-started,8)
        finally:
            timer.cancel()

    def test_cancel_endpoint_requires_header_and_targets_one_job(self):
        server=bridge.ThreadingHTTPServer(("127.0.0.1",0),bridge.Handler)
        worker=threading.Thread(target=server.serve_forever,daemon=True)
        worker.start()
        event=threading.Event()
        other=threading.Event()
        with bridge.JOBS_LOCK:
            bridge.JOBS.update({"test-job":event,"other-job":other})
        url=f"http://127.0.0.1:{server.server_port}/cancel"
        try:
            payload=json.dumps({"request_id":"test-job"}).encode()
            with self.assertRaises(urllib.error.HTTPError) as error:
                urllib.request.urlopen(urllib.request.Request(url,data=payload))
            self.assertEqual(error.exception.code,403)
            error.exception.close()
            request=urllib.request.Request(url,data=payload,headers={"X-AI-Builder":"cancel-request"})
            with urllib.request.urlopen(request) as response:
                self.assertTrue(json.load(response)["cancelled"])
            self.assertTrue(event.is_set())
            self.assertFalse(other.is_set())
        finally:
            with bridge.JOBS_LOCK:
                bridge.JOBS.pop("test-job",None)
                bridge.JOBS.pop("other-job",None)
            server.shutdown();server.server_close();worker.join()
