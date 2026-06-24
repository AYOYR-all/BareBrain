from __future__ import annotations

import argparse
import hashlib
import json
import shutil
import urllib.request
from pathlib import Path
from zipfile import ZipFile


def load_json(path: Path) -> dict:
    data = json.loads(path.read_text(encoding="utf-8-sig"))
    if not isinstance(data, dict):
        raise ValueError(f"{path} must contain a JSON object")
    return data


def download(url: str, destination: Path) -> None:
    request = urllib.request.Request(url, headers={"User-Agent": "BareBrain-Builder"})
    with urllib.request.urlopen(request, timeout=120) as response:
        destination.write_bytes(response.read())


def verify(path: Path, checksum: str) -> None:
    expected = checksum.removeprefix("sha256:")
    actual = hashlib.sha256(path.read_bytes()).hexdigest()
    if actual != expected:
        raise ValueError(f"checksum mismatch for {path.name}: expected {expected}, got {actual}")


def safe_extract(archive_path: Path, destination: Path) -> None:
    destination_root = destination.resolve()
    with ZipFile(archive_path) as archive:
        for item in archive.infolist():
            target = (destination / item.filename).resolve()
            try:
                target.relative_to(destination_root)
            except ValueError as exc:
                raise ValueError(f"unsafe archive path: {item.filename}") from exc
        archive.extractall(destination)


def main() -> int:
    parser = argparse.ArgumentParser(description="Download enabled BareBrain market Mods")
    parser.add_argument("--profile", type=Path, required=True)
    parser.add_argument("--index", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--download-dir", type=Path, required=True)
    args = parser.parse_args()

    profile = load_json(args.profile)
    index = load_json(args.index)
    enabled = set(profile.get("enabled_mods", []))
    locks = {
        item["id"]: item
        for item in profile.get("plugin_lock", [])
        if isinstance(item, dict) and isinstance(item.get("id"), str)
    }
    plugins = {
        item["id"]: item
        for item in index.get("plugins", [])
        if isinstance(item, dict) and isinstance(item.get("id"), str)
    }

    args.output_dir.mkdir(parents=True, exist_ok=True)
    args.download_dir.mkdir(parents=True, exist_ok=True)

    for plugin_id in sorted(enabled):
        lock = locks.get(plugin_id)
        if lock is None or lock.get("builtin") is True:
            continue
        plugin = plugins.get(plugin_id)
        if plugin is None:
            raise ValueError(f"enabled market plugin is missing from index: {plugin_id}")
        if plugin.get("version") != lock.get("version"):
            raise ValueError(
                f"{plugin_id}: profile locks {lock.get('version')} but index contains {plugin.get('version')}"
            )
        for field in ("repo", "archive", "checksum"):
            locked_value = lock.get(field)
            if locked_value is not None and locked_value != plugin.get(field):
                raise ValueError(f"{plugin_id}: locked {field} no longer matches the plugin index")
        archive_url = lock.get("archive") or plugin.get("archive")
        checksum = lock.get("checksum") or plugin.get("checksum")
        if not isinstance(archive_url, str) or not isinstance(checksum, str):
            raise ValueError(f"{plugin_id}: archive and checksum are required")

        archive_path = args.download_dir / f"{plugin_id}.zip"
        download(archive_url, archive_path)
        verify(archive_path, checksum)
        destination = args.output_dir / plugin_id
        if destination.exists():
            shutil.rmtree(destination)
        destination.mkdir(parents=True)
        safe_extract(archive_path, destination)

        manifest = load_json(destination / "barebrain.mod.json")
        if manifest.get("id") != plugin_id or manifest.get("version") != plugin.get("version"):
            raise ValueError(f"{plugin_id}: archive manifest does not match plugin index")
        print(f"Prepared {plugin_id} {plugin['version']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
