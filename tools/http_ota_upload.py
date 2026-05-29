#!/usr/bin/env python3
"""Upload AstroPixelsPlus firmware/SPIFFS images through the web OTA API."""

from __future__ import annotations

import argparse
import json
import mimetypes
import os
import sys
import time
import urllib.error
import urllib.parse
import urllib.request
import uuid


KINDS = {
    "firmware": {
        "endpoint": "/upload/firmware",
        "field": "firmware",
        "probe": "/api/health",
    },
    "filesystem": {
        "endpoint": "/upload/filesystem",
        "field": "filesystem",
        "probe": "/api/health",
    },
}


def normalize_base_url(host: str) -> str:
    if "://" not in host:
        host = "http://" + host
    parsed = urllib.parse.urlparse(host)
    if not parsed.scheme or not parsed.netloc:
        raise ValueError(f"invalid host/base URL: {host}")
    return host.rstrip("/")


def multipart_body(field: str, path: str) -> tuple[bytes, str]:
    boundary = "----AstroPixelsPlusOTA" + uuid.uuid4().hex
    filename = os.path.basename(path)
    content_type = mimetypes.guess_type(filename)[0] or "application/octet-stream"
    with open(path, "rb") as f:
        payload = f.read()

    head = (
        f"--{boundary}\r\n"
        f'Content-Disposition: form-data; name="{field}"; filename="{filename}"\r\n'
        f"Content-Type: {content_type}\r\n\r\n"
    ).encode("utf-8")
    tail = f"\r\n--{boundary}--\r\n".encode("utf-8")
    return head + payload + tail, boundary


def decode_response(data: bytes) -> dict:
    if not data:
        return {}
    try:
        return json.loads(data.decode("utf-8"))
    except (UnicodeDecodeError, json.JSONDecodeError):
        return {"raw": data.decode("utf-8", errors="replace")}


def upload(base_url: str, kind: str, path: str, timeout: float) -> None:
    meta = KINDS[kind]
    body, boundary = multipart_body(meta["field"], path)
    url = base_url + meta["endpoint"]
    req = urllib.request.Request(
        url,
        data=body,
        method="POST",
        headers={
            "Content-Type": f"multipart/form-data; boundary={boundary}",
            "Content-Length": str(len(body)),
        },
    )

    print(f"Uploading {kind} image: {path} ({os.path.getsize(path)} bytes)")
    print(f"POST {url}")
    try:
        with urllib.request.urlopen(req, timeout=timeout) as resp:
            status = resp.status
            payload = decode_response(resp.read())
    except urllib.error.HTTPError as exc:
        payload = decode_response(exc.read())
        error = payload.get("error") or payload.get("raw") or exc.reason
        raise RuntimeError(f"upload rejected with HTTP {exc.code}: {error}") from exc
    except urllib.error.URLError as exc:
        raise RuntimeError(f"upload failed: {exc.reason}") from exc

    if status != 200 or payload.get("ok") is not True:
        error = payload.get("error") or payload.get("raw") or f"unexpected response {payload!r}"
        raise RuntimeError(f"upload failed with HTTP {status}: {error}")

    print("Upload accepted; controller should reboot shortly.")


def wait_for_controller(base_url: str, probe_path: str, timeout: float) -> None:
    deadline = time.monotonic() + timeout
    url = base_url + probe_path
    attempt = 0
    time.sleep(1.5)
    while time.monotonic() < deadline:
        attempt += 1
        try:
            with urllib.request.urlopen(url, timeout=3) as resp:
                if 200 <= resp.status < 300:
                    print(f"Controller responded at {url}")
                    return
        except (urllib.error.URLError, TimeoutError):
            pass
        time.sleep(2)
    raise RuntimeError(f"controller did not respond at {url} within {timeout:.0f}s")


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("kind", choices=sorted(KINDS))
    parser.add_argument("--host", default="astropixelsplus.local",
                        help="controller host, IP, or base URL (default: astropixelsplus.local)")
    parser.add_argument("--file", required=True, help="image file to upload")
    parser.add_argument("--upload-timeout", type=float, default=180,
                        help="upload request timeout in seconds")
    parser.add_argument("--reboot-timeout", type=float, default=90,
                        help="time to wait for /api/health after upload")
    parser.add_argument("--no-wait", action="store_true",
                        help="do not wait for controller to come back online")
    return parser.parse_args(argv)


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    if not os.path.isfile(args.file):
        print(f"error: image file not found: {args.file}", file=sys.stderr)
        return 2

    try:
        base_url = normalize_base_url(args.host)
        upload(base_url, args.kind, args.file, args.upload_timeout)
        if not args.no_wait:
            wait_for_controller(base_url, KINDS[args.kind]["probe"], args.reboot_timeout)
    except Exception as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
