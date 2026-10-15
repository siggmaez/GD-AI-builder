"""Enable vendored OpenSSL for the native Android target (no login changes)."""
from pathlib import Path
import sys

root = Path(sys.argv[1])
manifest = root / 'core/Cargo.toml'
text = manifest.read_text()
for target in ('aarch64-linux-android', 'x86_64-linux-android'):
    text += f'\n[target.{target}.dependencies]\nopenssl-sys = {{ workspace = true, features = ["vendored"] }}\n'
manifest.write_text(text)
