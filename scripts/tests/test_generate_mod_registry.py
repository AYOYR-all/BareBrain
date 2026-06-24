from __future__ import annotations

import importlib.util
import json
import sys
import tempfile
import unittest
from pathlib import Path


SCRIPT = Path(__file__).resolve().parents[1] / "generate_mod_registry.py"
SPEC = importlib.util.spec_from_file_location("generate_mod_registry", SCRIPT)
assert SPEC and SPEC.loader
registry = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = registry
SPEC.loader.exec_module(registry)


def write_mod(
    root: Path,
    mod_id: str,
    *,
    symbol: str | None = None,
    idf_components: list[str] | None = None,
) -> None:
    mod_root = root / mod_id
    source = mod_root / "src" / f"mod_{mod_id.replace('-', '_')}.c"
    source.parent.mkdir(parents=True)
    source.write_text("/* fixture */\n", encoding="utf-8")
    manifest = {
        "schema": 1,
        "id": mod_id,
        "name": mod_id,
        "version": "1.0.0",
        "type": "tool",
        "description": "fixture",
        "targets": ["esp32s3"],
        "sources": [source.relative_to(mod_root).as_posix()],
    }
    if symbol:
        manifest["entry_symbol"] = symbol
    if idf_components:
        manifest["idf_components"] = idf_components
    (mod_root / "barebrain.mod.json").write_text(
        json.dumps(manifest),
        encoding="utf-8",
    )


class GenerateModRegistryTests(unittest.TestCase):
    def test_discovers_builtin_and_external_mods(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            builtin = root / "builtin"
            external = root / "external"
            write_mod(builtin, "tool-core")
            write_mod(external, "tool-display")

            mods = registry.discover(builtin, builtin=True) + registry.discover(external, builtin=False)

            self.assertEqual(["tool-core", "tool-display"], [mod.mod_id for mod in mods])
            self.assertTrue(mods[0].builtin)
            self.assertFalse(mods[1].builtin)
            self.assertEqual("brn_mod_tool_display", mods[1].symbol)

    def test_profile_filters_builtin_and_external_mods(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            profile = root / "profile.json"
            profile.write_text(
                json.dumps({"enabled_mods": ["tool-display"]}),
                encoding="utf-8",
            )

            self.assertEqual({"tool-display"}, registry.enabled_from_profile(profile))

    def test_rejects_source_outside_mod_directory(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            mod_root = root / "tool-bad"
            mod_root.mkdir()
            (root / "outside.c").write_text("/* fixture */\n", encoding="utf-8")
            (mod_root / "barebrain.mod.json").write_text(
                json.dumps({
                    "id": "tool-bad",
                    "sources": ["../outside.c"],
                }),
                encoding="utf-8",
            )

            with self.assertRaises(registry.RegistryError):
                registry.load_mod(mod_root / "barebrain.mod.json", builtin=False)

    def test_generated_registry_uses_entry_symbols(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            write_mod(root, "tool-display", symbol="custom_display_mod")
            mod = registry.discover(root, builtin=False)[0]

            source = registry.generate_source([mod])

            self.assertIn("extern const brn_mod_t custom_display_mod;", source)
            self.assertIn("&custom_display_mod,", source)
            self.assertIn("return 1U;", source)

    def test_generated_cmake_merges_idf_components(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            write_mod(root, "tool-display", idf_components=["esp_driver_i2c"])
            write_mod(root, "tool-tts", idf_components=["esp_driver_i2s", "esp_driver_i2c"])
            mods = registry.discover(root, builtin=False)

            cmake = registry.generate_cmake(mods)

            self.assertEqual(1, cmake.count("    esp_driver_i2c"))
            self.assertIn("    esp_driver_i2s", cmake)


if __name__ == "__main__":
    unittest.main()
