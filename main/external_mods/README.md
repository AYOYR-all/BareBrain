# External Mods

GitHub Actions or another build service extracts market plugins into this directory before CMake configuration.

Each direct child directory must contain a valid `barebrain.mod.json`:

```text
main/external_mods/
  tool-display/
    barebrain.mod.json
    src/
      mod_tool_display.c
      tool_display.c
      tool_display.h
```

Do not commit downloaded plugin contents to BareBrain. The build Profile selects which built-in and external Mods enter the firmware.

Configure with a Profile:

```powershell
idf.py -DBRN_BUILD_PROFILE=D:/build/firmware_profile.json reconfigure build
```

Or provide a comma-separated list:

```powershell
idf.py -DBRN_ENABLED_MODS=tool-get-time,tool-display reconfigure build
```

When neither option is provided, all built-in Mods are enabled and external Mods are disabled.
