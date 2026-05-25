# Web Search Tool

Registers `web_search`, backed by Tavily when configured and Brave Search as fallback.

Permissions: `network`, `secret.read`.

Configuration: search provider keys are stored in NVS through existing CLI configuration commands.

Known limits: results depend on network reachability and provider configuration.
