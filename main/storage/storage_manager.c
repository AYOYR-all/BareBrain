#include "storage/storage_manager.h"

#include "brn_config.h"
#include "storage/storage_fs.h"

#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "driver/gpio.h"
#include "driver/sdmmc_host.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"

static const char *TAG = "storage";

static bool s_spiffs_ready = false;
static bool s_sd_mounted = false;
static esp_err_t s_sd_last_error = ESP_OK;
static size_t s_spiffs_total = 0;
static size_t s_spiffs_used = 0;
static sdmmc_card_t *s_sd_card = NULL;
static const char *s_sd_interface = "disabled";

static gpio_num_t to_gpio_num(int pin)
{
    return pin < 0 ? GPIO_NUM_NC : (gpio_num_t)pin;
}

static esp_err_t mount_spiffs(void)
{
    esp_vfs_spiffs_conf_t conf = {
        .base_path = BRN_SPIFFS_BASE,
        .partition_label = NULL,
        .max_files = 10,
        .format_if_mount_failed = true,
    };

    ESP_RETURN_ON_ERROR(esp_vfs_spiffs_register(&conf), TAG, "SPIFFS mount failed");
    ESP_RETURN_ON_ERROR(esp_spiffs_info(NULL, &s_spiffs_total, &s_spiffs_used), TAG,
                        "SPIFFS info failed");

    s_spiffs_ready = true;
    ESP_LOGI(TAG, "SPIFFS mounted at %s: total=%d used=%d",
             BRN_SPIFFS_BASE, (int)s_spiffs_total, (int)s_spiffs_used);
    return ESP_OK;
}

static esp_err_t mount_sd_sdmmc(void)
{
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    sdmmc_slot_config_t slot = SDMMC_SLOT_CONFIG_DEFAULT();
    esp_vfs_fat_mount_config_t mount_config = VFS_FAT_MOUNT_DEFAULT_CONFIG();

    host.max_freq_khz = BRN_SD_MAX_FREQ_KHZ;
    if (BRN_SDMMC_BUS_WIDTH == 1) {
        host.flags &= ~SDMMC_HOST_FLAG_4BIT;
    }

    slot.width = BRN_SDMMC_BUS_WIDTH;
    slot.clk = to_gpio_num(BRN_SDMMC_PIN_CLK);
    slot.cmd = to_gpio_num(BRN_SDMMC_PIN_CMD);
    slot.d0 = to_gpio_num(BRN_SDMMC_PIN_D0);
    slot.d1 = to_gpio_num(BRN_SDMMC_PIN_D1);
    slot.d2 = to_gpio_num(BRN_SDMMC_PIN_D2);
    slot.d3 = to_gpio_num(BRN_SDMMC_PIN_D3);
    slot.cd = to_gpio_num(BRN_SDMMC_PIN_CD);
    slot.wp = to_gpio_num(BRN_SDMMC_PIN_WP);

    mount_config.max_files = BRN_SD_MAX_FILES;
    mount_config.format_if_mount_failed = BRN_SD_FORMAT_IF_MOUNT_FAILED;
    mount_config.allocation_unit_size = BRN_SD_ALLOCATION_UNIT_SIZE;
    return esp_vfs_fat_sdmmc_mount(BRN_SD_BASE, &host, &slot, &mount_config, &s_sd_card);
}

static void reset_sd_state(void)
{
    s_sd_mounted = false;
    s_sd_card = NULL;
}

static esp_err_t mount_sd(void)
{
    if (BRN_SD_MODE == BRN_SD_MODE_DISABLED) {
        s_sd_interface = "disabled";
        s_sd_last_error = ESP_OK;
        return ESP_OK;
    }

    s_sd_interface = "sdmmc";
    s_sd_last_error = mount_sd_sdmmc();
    if (s_sd_last_error != ESP_OK) {
        reset_sd_state();
        ESP_LOGW(TAG, "SD mount failed on %s: %s", s_sd_interface, esp_err_to_name(s_sd_last_error));
        return s_sd_last_error;
    }

    s_sd_mounted = true;
    ESP_LOGI(TAG, "SD mounted at %s via %s", BRN_SD_BASE, s_sd_interface);
    return ESP_OK;
}

static void ensure_sd_layout(void)
{
    static const char *dirs[] = {
        BRN_SD_BASE "/memory",
        BRN_SD_BASE "/sessions",
        BRN_SD_BASE "/docs",
    };

    for (size_t i = 0; i < sizeof(dirs) / sizeof(dirs[0]); i++) {
        char probe_path[128];
        snprintf(probe_path, sizeof(probe_path), "%s/.probe", dirs[i]);
        if (storage_fs_ensure_parent_dir(probe_path) != ESP_OK) {
            ESP_LOGW(TAG, "Failed to prepare %s", dirs[i]);
        }
    }
}

static void migrate_prefix(const char *prefix)
{
    DIR *dir = opendir(BRN_SPIFFS_BASE);
    if (!dir) {
        return;
    }

    struct dirent *ent = NULL;
    while ((ent = readdir(dir)) != NULL) {
        struct stat st = {0};
        char src_path[512];
        char dst_path[512];

        if (strncmp(ent->d_name, prefix, strlen(prefix)) != 0) {
            continue;
        }

        snprintf(src_path, sizeof(src_path), "%s/%s", BRN_SPIFFS_BASE, ent->d_name);
        snprintf(dst_path, sizeof(dst_path), "%s/%s", BRN_SD_BASE, ent->d_name);
        if (stat(dst_path, &st) == 0) {
            continue;
        }
        if (storage_fs_copy_file_if_missing(src_path, dst_path) == ESP_OK) {
            ESP_LOGI(TAG, "Migrated %s -> %s", src_path, dst_path);
        }
    }

    closedir(dir);
}

static void migrate_data_to_sd(void)
{
    static const char *prefixes[] = {
        "sessions/",
        "docs/",
    };

    if (!storage_data_on_sd()) {
        return;
    }

    for (size_t i = 0; i < sizeof(prefixes) / sizeof(prefixes[0]); i++) {
        migrate_prefix(prefixes[i]);
    }
}

esp_err_t storage_init(void)
{
    ESP_RETURN_ON_ERROR(mount_spiffs(), TAG, "core storage init failed");
    mount_sd();

    if (storage_data_on_sd()) {
        ensure_sd_layout();
        migrate_data_to_sd();
    }

    storage_log_status();
    return ESP_OK;
}

const char *storage_get_data_base(void)
{
    return storage_data_on_sd() ? BRN_SD_BASE : BRN_SPIFFS_BASE;
}

bool storage_data_on_sd(void)
{
    return BRN_SD_PREFER_DATA && s_sd_mounted;
}

bool storage_sd_is_enabled(void)
{
    return BRN_SD_MODE != BRN_SD_MODE_DISABLED;
}

bool storage_sd_is_mounted(void)
{
    return s_sd_mounted;
}

void storage_get_status(brn_storage_status_t *status)
{
    if (!status) {
        return;
    }

    status->spiffs_ready = s_spiffs_ready;
    status->spiffs_total_bytes = s_spiffs_total;
    status->spiffs_used_bytes = s_spiffs_used;
    status->sd_enabled = storage_sd_is_enabled();
    status->sd_mounted = s_sd_mounted;
    status->data_on_sd = storage_data_on_sd();
    status->sd_last_error = s_sd_last_error;
    status->data_base = storage_get_data_base();
    status->sd_interface = s_sd_interface;
}

void storage_log_status(void)
{
    ESP_LOGI(TAG, "Storage status: data_base=%s sd_enabled=%s sd_mounted=%s",
             storage_get_data_base(),
             storage_sd_is_enabled() ? "yes" : "no",
             s_sd_mounted ? "yes" : "no");
}

void storage_print_sd_info(FILE *stream)
{
    if (s_sd_card) {
        sdmmc_card_print_info(stream, s_sd_card);
    }
}
