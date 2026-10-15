"""Exercise the real native runtime and local HTTP bridge in an Android emulator."""
import json
from pathlib import Path
import subprocess
import sys
import time
import urllib.error
import urllib.request
import xml.etree.ElementTree as ET

PACKAGE = 'com.siggmaez.aibuilder.bridge'
def adb(*args):
    return subprocess.check_output(['adb', *args], text=True)

def request(path):
    with urllib.request.urlopen('http://127.0.0.1:18765'+path, timeout=5) as response:
        return json.load(response)

def screenshot():
    Path('phone-bridge.png').write_bytes(subprocess.check_output(['adb','exec-out','screencap','-p']))

try:
    adb('install','-r',sys.argv[1])
    adb('shell','pm','grant',PACKAGE,'android.permission.POST_NOTIFICATIONS')
    adb('forward','tcp:18765','tcp:8765')
    adb('shell','am','start','-n',PACKAGE+'/.MainActivity','-a','android.intent.action.VIEW','-d','aibuilder://bridge/start')
    deadline=time.monotonic()+120
    while True:
        try:
            assert request('/health')['status']=='ok'
            break
        except (OSError, AssertionError):
            if time.monotonic()>deadline:
                raise RuntimeError('Native Codex or embedded bridge failed to start in Android')
            time.sleep(1)
    assert request('/models')['models']
    assert request('/auth/status')['state']=='idle'
    assert request('/limits')['available'] is False  # no account or API keys in CI
    screenshot()
    adb('shell','uiautomator','dump','/sdcard/window.xml')
    xml=adb('shell','cat','/sdcard/window.xml')
    assert 'Bridge online' in xml
    root=ET.fromstring(xml)
    import re
    stop=next(node for node in root.iter('node') if node.get('text')=='Stop bridge')
    coords=list(map(int,re.findall(r'\d+',stop.get('bounds'))))
    adb('shell','input','tap',str((coords[0]+coords[2])//2),str((coords[1]+coords[3])//2))
    time.sleep(3)
    try:
        request('/health')
    except OSError:
        pass
    else:
        raise AssertionError('Stop button did not stop the local server')
    print('Android smoke passed: native Codex starts, health/models/limits/account work, Stop stops bridge.')
except Exception:
    screenshot()
    print(adb('logcat','-d','-s','AndroidRuntime:E','python.stderr:V','python.stdout:V'))
    raise
