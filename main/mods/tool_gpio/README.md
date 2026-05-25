# GPIO Tools

Registers `gpio_write`, `gpio_read`, and `gpio_read_all` for hardware GPIO access constrained by the firmware GPIO policy.

Permissions: `gpio.read`, `gpio.write`.

Configuration: allowed pins and directions are defined by the GPIO policy.

Known limits: protected boot, flash, SD, and console pins remain blocked by policy.
