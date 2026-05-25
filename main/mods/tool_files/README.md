# File Tools

Registers `read_file`, `write_file`, `edit_file`, and `list_dir` for local storage paths under the configured SPIFFS and SD roots.

Permissions: `storage.read`, `storage.write`.

Configuration: storage roots are provided by the firmware configuration.

Known limits: path validation is enforced by the tool implementation.
