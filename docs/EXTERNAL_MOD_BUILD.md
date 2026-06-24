# External Mod Build

BareBrain discovers build-time Mods from two locations:

- `main/mods`: built-in Mods shipped with BareBrain.
- `main/external_mods`: market Mods downloaded by the build service.

Each direct child directory must contain `barebrain.mod.json`. The manifest `sources` and `headers` fields are resolved relative to that Mod directory. A Mod entry symbol defaults to:

```text
brn_mod_<mod-id-with-underscores>
```

For example, `tool-display` exports:

```c
const brn_mod_t brn_mod_tool_display;
```

Use the optional manifest field `entry_symbol` only when an existing plugin cannot follow that convention.

External Mods can declare additional ESP-IDF component dependencies:

```json
{
  "idf_components": [
    "esp_driver_i2c",
    "esp_driver_i2s"
  ]
}
```

The generator merges these values into the main component `REQUIRES` list.

## Build With A Firmware Profile

The firmware Profile is the preferred input:

```json
{
  "enabled_mods": [
    "tool-get-time",
    "tool-display"
  ]
}
```

Configure and build:

```powershell
idf.py -DBRN_BUILD_PROFILE=D:/build/firmware_profile.json reconfigure build
```

The generator fails configuration when an enabled Mod cannot be found. Only selected Mod source files are compiled and only selected Mod symbols enter the generated runtime registry.

For local testing without a Profile:

```powershell
idf.py -DBRN_ENABLED_MODS=tool-get-time,tool-display reconfigure build
```

When neither option is set, BareBrain enables every built-in Mod and disables every external Mod. This preserves the original development build.

## GitHub Actions Order

The cloud workflow should:

1. Check out BareBrain.
2. Read `enabled_mods` and `plugin_lock` from the submitted Profile.
3. Download each enabled market plugin archive.
4. Verify its SHA-256 checksum.
5. Extract it to `main/external_mods/<plugin-id>`.
6. Run `idf.py -DBRN_BUILD_PROFILE=<profile-path> reconfigure build`.
7. Package and upload the firmware artifact.

Downloaded plugin directories are build inputs and should not be committed to BareBrain.
