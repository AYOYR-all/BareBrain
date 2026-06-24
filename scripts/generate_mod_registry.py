#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import re
import sys
from dataclasses import dataclass
from pathlib import Path


ID_RE = re.compile(r"^[a-z0-9]+(?:-[a-z0-9]+)*$")


class RegistryError(RuntimeError):
    pass


@dataclass(frozen=True)
class Mod:
    mod_id: str
    root: Path
    manifest: Path
    sources: tuple[Path, ...]
    include_dirs: tuple[Path, ...]
    idf_components: tuple[str, ...]
    symbol: str
    builtin: bool


def load_json(path: Path) -> dict:
    try:
        data = json.loads(path.read_text(encoding="utf-8-sig"))
    except (OSError, json.JSONDecodeError) as exc:
        raise RegistryError(f"cannot read {path}: {exc}") from exc
    if not isinstance(data, dict):
        raise RegistryError(f"{path}: manifest must be a JSON object")
    return data


def resolve_inside(root: Path, relative: str, field: str) -> Path:
    path = (root / relative).resolve()
    try:
        path.relative_to(root.resolve())
    except ValueError as exc:
        raise RegistryError(f"{root}: {field} escapes the mod directory: {relative}") from exc
    if not path.is_file():
        raise RegistryError(f"{root}: {field} does not exist: {relative}")
    return path


def load_mod(manifest: Path, *, builtin: bool) -> Mod:
    data = load_json(manifest)
    if data.get("schema") != 1:
        raise RegistryError(f"{manifest}: schema must be 1")
    mod_id = data.get("id")
    if not isinstance(mod_id, str) or not ID_RE.fullmatch(mod_id):
        raise RegistryError(f"{manifest}: id must be lowercase kebab-case")
    targets = data.get("targets")
    if not isinstance(targets, list) or "esp32s3" not in targets:
        raise RegistryError(f"{manifest}: targets must include esp32s3")

    raw_sources = data.get("sources")
    if not isinstance(raw_sources, list) or not raw_sources:
        raise RegistryError(f"{manifest}: sources must be a non-empty list")

    root = manifest.parent.resolve()
    sources = tuple(resolve_inside(root, item, "source") for item in raw_sources if isinstance(item, str))
    if len(sources) != len(raw_sources):
        raise RegistryError(f"{manifest}: every source must be a string")

    include_dirs: set[Path] = {root}
    for source in sources:
        include_dirs.add(source.parent)

    raw_headers = data.get("headers", [])
    if not isinstance(raw_headers, list) or not all(isinstance(item, str) for item in raw_headers):
        raise RegistryError(f"{manifest}: headers must be a string list")
    for header in raw_headers:
        include_dirs.add(resolve_inside(root, header, "header").parent)

    raw_components = data.get("idf_components", [])
    if not isinstance(raw_components, list) or not all(
        isinstance(item, str) and re.fullmatch(r"[A-Za-z0-9_.-]+", item)
        for item in raw_components
    ):
        raise RegistryError(f"{manifest}: idf_components must be a component name list")

    symbol = data.get("entry_symbol", f"brn_mod_{mod_id.replace('-', '_')}")
    if not isinstance(symbol, str) or not re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", symbol):
        raise RegistryError(f"{manifest}: entry_symbol is not a valid C identifier")

    return Mod(
        mod_id=mod_id,
        root=root,
        manifest=manifest.resolve(),
        sources=sources,
        include_dirs=tuple(sorted(include_dirs)),
        idf_components=tuple(raw_components),
        symbol=symbol,
        builtin=builtin,
    )


def discover(root: Path, *, builtin: bool) -> list[Mod]:
    if not root.exists():
        return []
    return [
        load_mod(manifest, builtin=builtin)
        for manifest in sorted(root.glob("*/barebrain.mod.json"))
    ]


def parse_enabled(raw: str | None) -> set[str] | None:
    if raw is None or not raw.strip():
        return None
    return {item.strip() for item in re.split(r"[,;]", raw) if item.strip()}


def enabled_from_profile(path: Path | None) -> set[str] | None:
    if path is None:
        return None
    profile = load_json(path)
    enabled = profile.get("enabled_mods")
    if not isinstance(enabled, list) or not all(isinstance(item, str) for item in enabled):
        raise RegistryError(f"{path}: enabled_mods must be a string list")
    return set(enabled)


def cmake_quote(path: Path) -> str:
    return '"' + path.as_posix().replace('"', '\\"') + '"'


def write_if_changed(path: Path, content: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    if path.exists() and path.read_text(encoding="utf-8") == content:
        return
    path.write_text(content, encoding="utf-8", newline="\n")


def generate_cmake(mods: list[Mod]) -> str:
    sources = "\n".join(f"    {cmake_quote(source)}" for mod in mods for source in mod.sources)
    include_dirs = sorted({directory for mod in mods for directory in mod.include_dirs})
    includes = "\n".join(f"    {cmake_quote(directory)}" for directory in include_dirs)
    components = sorted({component for mod in mods for component in mod.idf_components})
    requires = "\n".join(f"    {component}" for component in components)
    return (
        "# Generated by scripts/generate_mod_registry.py. Do not edit.\n"
        "set(BRN_ENABLED_MOD_SOURCES\n"
        f"{sources}\n"
        ")\n\n"
        "set(BRN_ENABLED_MOD_INCLUDE_DIRS\n"
        f"{includes}\n"
        ")\n\n"
        "set(BRN_ENABLED_MOD_REQUIRES\n"
        f"{requires}\n"
        ")\n"
    )


def generate_header() -> str:
    return """// Generated by scripts/generate_mod_registry.py. Do not edit.
#pragma once

#include <stddef.h>

#include "core/mod/brn_mod.h"

const brn_mod_t *const *brn_mod_registry_get(void);
size_t brn_mod_registry_count(void);
"""


def generate_source(mods: list[Mod]) -> str:
    externs = "\n".join(f"extern const brn_mod_t {mod.symbol};" for mod in mods)
    entries = "\n".join(f"    &{mod.symbol}," for mod in mods)
    return f"""// Generated by scripts/generate_mod_registry.py. Do not edit.
#include "generated/mod_registry.h"

{externs}

static const brn_mod_t *const s_mods[] = {{
{entries}
    NULL,
}};

const brn_mod_t *const *brn_mod_registry_get(void)
{{
    return s_mods;
}}

size_t brn_mod_registry_count(void)
{{
    return {len(mods)}U;
}}
"""


def main() -> int:
    parser = argparse.ArgumentParser(description="Generate BareBrain enabled Mod sources and registry")
    parser.add_argument("--builtin-dir", type=Path, required=True)
    parser.add_argument("--external-dir", type=Path, required=True)
    parser.add_argument("--profile", type=Path)
    parser.add_argument("--enabled-mods")
    parser.add_argument("--out-cmake", type=Path, required=True)
    parser.add_argument("--out-c", type=Path, required=True)
    parser.add_argument("--out-h", type=Path, required=True)
    args = parser.parse_args()

    try:
        mods = discover(args.builtin_dir, builtin=True) + discover(args.external_dir, builtin=False)
        by_id: dict[str, Mod] = {}
        for mod in mods:
            if mod.mod_id in by_id:
                raise RegistryError(
                    f"duplicate mod id {mod.mod_id}: {by_id[mod.mod_id].manifest} and {mod.manifest}"
                )
            by_id[mod.mod_id] = mod

        profile_enabled = enabled_from_profile(args.profile)
        cli_enabled = parse_enabled(args.enabled_mods)
        if profile_enabled is not None and cli_enabled is not None:
            raise RegistryError("use either --profile or --enabled-mods, not both")
        enabled = profile_enabled if profile_enabled is not None else cli_enabled

        if enabled is None:
            selected = [mod for mod in mods if mod.builtin]
        else:
            missing = sorted(enabled - set(by_id))
            if missing:
                raise RegistryError(f"enabled mods were not found: {', '.join(missing)}")
            selected = [mod for mod in mods if mod.mod_id in enabled]

        selected.sort(key=lambda mod: (not mod.builtin, mod.mod_id))
        write_if_changed(args.out_cmake, generate_cmake(selected))
        write_if_changed(args.out_h, generate_header())
        write_if_changed(args.out_c, generate_source(selected))
        print("Enabled BareBrain mods: " + (", ".join(mod.mod_id for mod in selected) or "(none)"))
        return 0
    except RegistryError as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
