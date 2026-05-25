#pragma once

/* BareBrain Global Configuration */

/* Build-time secrets (highest priority, override NVS) */
#if __has_include("brn_secrets.h")
#include "brn_secrets.h"
#endif

#ifndef BRN_SECRET_WIFI_SSID
#define BRN_SECRET_WIFI_SSID       ""
#endif
#ifndef BRN_SECRET_WIFI_PASS
#define BRN_SECRET_WIFI_PASS       ""
#endif
#ifndef BRN_SECRET_API_KEY
#define BRN_SECRET_API_KEY         ""
#endif
#ifndef BRN_SECRET_MODEL
#define BRN_SECRET_MODEL           ""
#endif
#ifndef BRN_SECRET_MODEL_PROVIDER
#define BRN_SECRET_MODEL_PROVIDER  "anthropic"
#endif
#ifndef BRN_SECRET_BASE_URL
#define BRN_SECRET_BASE_URL        ""
#endif
#ifndef BRN_SECRET_PROXY_HOST
#define BRN_SECRET_PROXY_HOST      ""
#endif
#ifndef BRN_SECRET_PROXY_PORT
#define BRN_SECRET_PROXY_PORT      ""
#endif
#ifndef BRN_SECRET_PROXY_TYPE
#define BRN_SECRET_PROXY_TYPE      ""
#endif
#ifndef BRN_SECRET_SEARCH_KEY
#define BRN_SECRET_SEARCH_KEY      ""
#endif
#ifndef BRN_SECRET_FEISHU_APP_ID
#define BRN_SECRET_FEISHU_APP_ID   ""
#endif
#ifndef BRN_SECRET_FEISHU_APP_SECRET
#define BRN_SECRET_FEISHU_APP_SECRET ""
#endif
#ifndef BRN_SECRET_TAVILY_KEY
#define BRN_SECRET_TAVILY_KEY      ""
#endif
#ifndef BRN_SECRET_MEMORY_API_KEY
#define BRN_SECRET_MEMORY_API_KEY  ""
#endif
#ifndef BRN_SECRET_MEMORY_MODEL
#define BRN_SECRET_MEMORY_MODEL    ""
#endif
#ifndef BRN_SECRET_MEMORY_PROVIDER
#define BRN_SECRET_MEMORY_PROVIDER ""
#endif
#ifndef BRN_SECRET_MEMORY_BASE_URL
#define BRN_SECRET_MEMORY_BASE_URL ""
#endif

/* WiFi */
#define BRN_WIFI_MAX_RETRY          10
#define BRN_WIFI_RETRY_BASE_MS      1000
#define BRN_WIFI_RETRY_MAX_MS       30000

/* Feishu Bot */
#define BRN_FEISHU_MAX_MSG_LEN          4096
#define BRN_FEISHU_POLL_STACK           (12 * 1024)
#define BRN_FEISHU_POLL_PRIO            5
#define BRN_FEISHU_POLL_CORE            0
#define BRN_FEISHU_WEBHOOK_PORT         18790
#define BRN_FEISHU_WEBHOOK_PATH         "/feishu/events"
#define BRN_FEISHU_WEBHOOK_MAX_BODY     (16 * 1024)

/* Agent Loop */
#define BRN_AGENT_STACK             (24 * 1024)
#define BRN_AGENT_PRIO              6
#define BRN_AGENT_CORE              1
#define BRN_AGENT_MAX_HISTORY       20
#define BRN_AGENT_MAX_TOOL_ITER     10
#define BRN_MAX_TOOL_CALLS          4
#define BRN_AGENT_SEND_WORKING_STATUS 1

/* Timezone (POSIX TZ format, China Standard Time / UTC+8) */
#define BRN_TIMEZONE                "CST-8"

/* LLM */
#define BRN_LLM_DEFAULT_MODEL       "claude-opus-4-5"
#define BRN_LLM_PROVIDER_DEFAULT    "anthropic"
#define BRN_LLM_MAX_TOKENS          4096
#define BRN_ANTHROPIC_BASE_URL      "https://api.anthropic.com/v1"
#define BRN_OPENAI_BASE_URL         "https://api.openai.com/v1"
#define BRN_LLM_API_VERSION         "2023-06-01"
#define BRN_LLM_STREAM_BUF_SIZE     (32 * 1024)
#define BRN_LLM_LOG_VERBOSE_PAYLOAD 0
#define BRN_LLM_LOG_PREVIEW_BYTES   160

/* Memory Indexing */
#define BRN_MEMORY_MAX_NODES            64
#define BRN_MEMORY_MAX_TAGS             6
#define BRN_MEMORY_MAX_LINKS            8
#define BRN_MEMORY_ID_LEN               24
#define BRN_MEMORY_KIND_LEN             16
#define BRN_MEMORY_TITLE_LEN            72
#define BRN_MEMORY_SUMMARY_LEN          224
#define BRN_MEMORY_TAG_LEN              24
#define BRN_MEMORY_PATH_LEN             160
#define BRN_MEMORY_PROMPT_LIMIT         8
#define BRN_MEMORY_CATALOG_LIMIT        12
#define BRN_MEMORY_DEFAULT_SEARCH_LIMIT 6
#define BRN_MEMORY_WORKER_STACK         (14 * 1024)
#define BRN_MEMORY_WORKER_PRIO          4
#define BRN_MEMORY_WORKER_CORE          0
#define BRN_MEMORY_WORKER_INTERVAL_MS   5000
#define BRN_MEMORY_MODEL_MAX_TOKENS     900

/* Message Bus */
#define BRN_BUS_QUEUE_LEN           16
#define BRN_OUTBOUND_STACK          (12 * 1024)
#define BRN_OUTBOUND_PRIO           5
#define BRN_OUTBOUND_CORE           0

/* Storage */
#define BRN_SPIFFS_BASE             "/spiffs"
#define BRN_SPIFFS_CONFIG_DIR       BRN_SPIFFS_BASE "/config"
#define BRN_SPIFFS_SESSION_DIR      BRN_SPIFFS_BASE "/sessions"
#define BRN_SD_BASE                 "/sdcard"
#define BRN_SD_MODE_DISABLED        0
#define BRN_SD_MODE_SDMMC           1
/* BRN_SD_MODE supports only disabled or SDMMC / SDIO 4-bit. */
#define BRN_SD_MODE                 BRN_SD_MODE_SDMMC
#define BRN_SD_PREFER_DATA          1
#define BRN_SD_MAX_FILES            8
#define BRN_SD_ALLOCATION_UNIT_SIZE (16 * 1024)
#define BRN_SD_FORMAT_IF_MOUNT_FAILED 0
#define BRN_SD_MAX_FREQ_KHZ         20000
#define BRN_SDMMC_BUS_WIDTH         4
#define BRN_SDMMC_PIN_CLK           14
#define BRN_SDMMC_PIN_CMD           15
#define BRN_SDMMC_PIN_D0            2
#define BRN_SDMMC_PIN_D1            4
#define BRN_SDMMC_PIN_D2            12
#define BRN_SDMMC_PIN_D3            13
#define BRN_SDMMC_PIN_CD            -1
#define BRN_SDMMC_PIN_WP            -1
#define BRN_SOUL_FILE               BRN_SPIFFS_CONFIG_DIR "/SOUL.md"
#define BRN_USER_FILE               BRN_SPIFFS_CONFIG_DIR "/USER.md"
#define BRN_CONTEXT_BUF_SIZE        (16 * 1024)
#define BRN_SESSION_MAX_MSGS        20

/* Cron / Heartbeat */
#define BRN_CRON_FILE               BRN_SPIFFS_BASE "/cron.json"
#define BRN_CRON_MAX_JOBS           16
#define BRN_CRON_CHECK_INTERVAL_MS  (60 * 1000)
#define BRN_HEARTBEAT_FILE          BRN_SPIFFS_BASE "/HEARTBEAT.md"
#define BRN_HEARTBEAT_INTERVAL_MS   (30 * 60 * 1000)

/* GPIO */
#define BRN_GPIO_CONFIG_SECTION     1   /* enable GPIO tools */

/* Skills */
#define BRN_SKILLS_PREFIX           BRN_SPIFFS_BASE "/skills/"

/* WebSocket Gateway */
#define BRN_WS_PORT                 18789
#define BRN_WS_MAX_CLIENTS          4

/* Serial CLI */
#define BRN_CLI_STACK               (4 * 1024)
#define BRN_CLI_PRIO                3
#define BRN_CLI_CORE                0

/* NVS Namespaces */
#define BRN_NVS_WIFI                "wifi_config"
#define BRN_NVS_FEISHU              "feishu_config"
#define BRN_NVS_LLM                 "llm_config"
#define BRN_NVS_PROXY               "proxy_config"
#define BRN_NVS_SEARCH              "search_config"
#define BRN_NVS_MEMORY_LLM          "memory_llm"

/* NVS Keys */
#define BRN_NVS_KEY_SSID            "ssid"
#define BRN_NVS_KEY_PASS            "password"
#define BRN_NVS_KEY_FEISHU_APP_ID   "app_id"
#define BRN_NVS_KEY_FEISHU_APP_SECRET "app_secret"
#define BRN_NVS_KEY_API_KEY         "api_key"
#define BRN_NVS_KEY_TAVILY_KEY      "tavily_key"
#define BRN_NVS_KEY_MODEL           "model"
#define BRN_NVS_KEY_PROVIDER        "provider"
#define BRN_NVS_KEY_BASE_URL        "base_url"
#define BRN_NVS_KEY_PROXY_HOST      "host"
#define BRN_NVS_KEY_PROXY_PORT      "port"
#define BRN_NVS_KEY_PROXY_TYPE      "proxy_type"
#define BRN_NVS_KEY_MEMORY_API_KEY  "memory_api_key"
#define BRN_NVS_KEY_MEMORY_MODEL    "memory_model"
#define BRN_NVS_KEY_MEMORY_PROVIDER "memory_provider"
#define BRN_NVS_KEY_MEMORY_BASE_URL "memory_base_url"

/* WiFi Onboarding (Captive Portal) */
#define BRN_ONBOARD_AP_PREFIX    "BRN-"
#define BRN_ONBOARD_AP_PASS      ""          /* open network */
#define BRN_ONBOARD_HTTP_PORT    80
#define BRN_ONBOARD_DNS_STACK    (4 * 1024)
#define BRN_ONBOARD_MAX_SCAN     20
