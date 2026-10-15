"""Validated CLI models, context history and temporary reference images."""
import base64
import binascii
import json
import os
from pathlib import Path
import re
import tempfile
from contextlib import contextmanager

MAX_IMAGE = 5 * 1024 * 1024


def model_list(cli="codex"):
    models = [{"id": "", "name": "CLI default", "images": cli == "codex"}]
    if cli != "codex":
        return models
    try:
        home = Path(os.environ.get("CODEX_HOME", str(Path.home() / ".codex")))
        data = json.loads((home / "models_cache.json").read_text(encoding="utf-8"))
        for m in data.get("models", []):
            slug = m.get("slug", "")
            if not valid_model(slug) or not slug or m.get("visibility") == "hidden":
                continue
            models.append({"id": slug, "name": str(m.get("display_name", slug))[:100],
                           "images": "image" in m.get("input_modalities", [])})
    except (OSError, ValueError, TypeError, AttributeError):
        pass
    return models


def valid_model(model):
    return isinstance(model, str) and (not model or re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9._:/-]{0,99}", model) is not None)


def history_messages(value):
    if not isinstance(value, list):
        raise ValueError("History must be an array")
    return [{"role": m["role"], "text": m["text"][:4000]}
            for m in value[-20:] if isinstance(m, dict) and
            m.get("role") in ("user", "assistant") and isinstance(m.get("text"), str)]


def validate_image(image):
    # Older mod versions serialize an unset matjson value as an empty object.
    if image is None or image == {}:
        return None
    if not isinstance(image, dict) or not isinstance(image.get("data"), str):
        raise ValueError("Invalid image attachment")
    if len(image["data"]) > (MAX_IMAGE + 2) // 3 * 4:
        raise ValueError("Image exceeds 5 MB")
    try:
        raw = base64.b64decode(image["data"], validate=True)
    except (ValueError, binascii.Error):
        raise ValueError("Invalid image encoding")
    if not raw or len(raw) > MAX_IMAGE:
        raise ValueError("Image exceeds 5 MB or is empty")
    if raw.startswith(b"\x89PNG\r\n\x1a\n"):
        ext = ".png"
    elif raw.startswith(b"\xff\xd8\xff"):
        ext = ".jpg"
    else:
        raise ValueError("Choose a PNG or JPEG image")
    return {"bytes": raw, "extension": ext}


@contextmanager
def image_file(image):
    if image is None:
        yield None
        return
    with tempfile.TemporaryDirectory(prefix="gd-ai-reference-") as folder:
        path = Path(folder) / ("reference" + image["extension"])
        path.write_bytes(image["bytes"])
        yield str(path)
