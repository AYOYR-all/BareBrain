find_package(Python3 REQUIRED COMPONENTS Interpreter)

get_filename_component(BRN_PROJECT_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

set(BRN_EXTERNAL_MODS_DIR "${CMAKE_CURRENT_LIST_DIR}/external_mods" CACHE PATH
    "Directory containing downloaded BareBrain market Mods")
set(BRN_BUILD_PROFILE "" CACHE FILEPATH
    "Firmware Profile JSON containing enabled_mods")
set(BRN_ENABLED_MODS "" CACHE STRING
    "Comma-separated enabled Mod IDs; ignored when BRN_BUILD_PROFILE is set")

set(BRN_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(BRN_GENERATED_CMAKE "${BRN_GENERATED_DIR}/enabled_mods.cmake")
set(BRN_GENERATED_REGISTRY_C "${BRN_GENERATED_DIR}/mod_registry.c")
set(BRN_GENERATED_REGISTRY_H "${BRN_GENERATED_DIR}/mod_registry.h")

set(BRN_MOD_GENERATOR_ARGS
    --builtin-dir "${CMAKE_CURRENT_LIST_DIR}/mods"
    --external-dir "${BRN_EXTERNAL_MODS_DIR}"
    --out-cmake "${BRN_GENERATED_CMAKE}"
    --out-c "${BRN_GENERATED_REGISTRY_C}"
    --out-h "${BRN_GENERATED_REGISTRY_H}")

if(BRN_BUILD_PROFILE)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${BRN_BUILD_PROFILE}")
    list(APPEND BRN_MOD_GENERATOR_ARGS --profile "${BRN_BUILD_PROFILE}")
elseif(BRN_ENABLED_MODS)
    list(APPEND BRN_MOD_GENERATOR_ARGS --enabled-mods "${BRN_ENABLED_MODS}")
endif()

execute_process(
    COMMAND "${Python3_EXECUTABLE}"
        "${BRN_PROJECT_ROOT}/scripts/generate_mod_registry.py"
        ${BRN_MOD_GENERATOR_ARGS}
    RESULT_VARIABLE BRN_MOD_GENERATOR_RESULT
    OUTPUT_VARIABLE BRN_MOD_GENERATOR_OUTPUT
    ERROR_VARIABLE BRN_MOD_GENERATOR_ERROR
)

if(NOT BRN_MOD_GENERATOR_RESULT EQUAL 0)
    message(FATAL_ERROR
        "BareBrain Mod registry generation failed:\n${BRN_MOD_GENERATOR_ERROR}")
endif()
string(STRIP "${BRN_MOD_GENERATOR_OUTPUT}" BRN_MOD_GENERATOR_OUTPUT)
message(STATUS "${BRN_MOD_GENERATOR_OUTPUT}")

include("${BRN_GENERATED_CMAKE}")

idf_component_register(
    SRCS
        "brn.c"
        "bus/message_bus.c"
        "wifi/wifi_manager.c"
        "channels/feishu/feishu_bot.c"
        "llm/llm_endpoint.c"
        "llm/llm_proxy.c"
        "agent/agent_loop.c"
        "agent/context_builder.c"
        "memory/memory_store.c"
        "memory/memory_index.c"
        "memory/memory_model.c"
        "memory/memory_worker_item.c"
        "memory/memory_worker.c"
        "memory/session_mgr.c"
        "storage/storage_manager.c"
        "storage/storage_fs.c"
        "gateway/ws_server.c"
        "cli/serial_cli.c"
        "proxy/http_proxy.c"
        "cron/cron_service.c"
        "heartbeat/heartbeat.c"
        "core/mod/brn_mod_manager.c"
        "tools/tool_registry.c"
        "${BRN_GENERATED_REGISTRY_C}"
        ${BRN_ENABLED_MOD_SOURCES}
        "tools/gpio_policy.c"
        "skills/skill_loader.c"
        "onboard/wifi_onboard.c"
    INCLUDE_DIRS
        "."
        "${CMAKE_CURRENT_BINARY_DIR}"
        ${BRN_ENABLED_MOD_INCLUDE_DIRS}
    REQUIRES
        nvs_flash esp_wifi esp_netif esp_http_client esp_http_server
        esp_https_ota esp_event json spiffs fatfs console vfs app_update esp-tls
        esp_timer esp_websocket_client esp_driver_gpio esp_driver_uart esp_driver_sdmmc sdmmc
        ${BRN_ENABLED_MOD_REQUIRES}
)
