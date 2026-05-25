# Cron Tools

Registers `cron_add`, `cron_list`, and `cron_remove` for scheduled agent work.

Permissions: `timer`, `agent.inject`.

Configuration: scheduler behavior comes from the cron service.

Known limits: execution depends on the cron service being initialized and started.
