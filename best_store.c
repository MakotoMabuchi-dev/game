#include <stdio.h>
#include <string.h>
#include "ff.h"
#include "f_util.h"
#include "hw_config.h"
#include "sd_card.h"
#include "best_store.h"

#define BEST_STORE_FILENAME "best_records.txt"
#define BEST_STORE_MAX_ENTRIES 16
#define BEST_STORE_KEY_SIZE 16

typedef struct {
    char key[BEST_STORE_KEY_SIZE];
    bool has_value;
    uint32_t value;
} best_store_entry_t;

static FATFS g_best_fs;
static bool g_best_mounted = false;
static bool g_best_loaded = false;
static best_store_entry_t g_best_entries[BEST_STORE_MAX_ENTRIES];
static size_t g_best_entry_count = 0;

static best_store_entry_t *best_store_find_entry(const char *key)
{
    for (size_t i = 0; i < g_best_entry_count; ++i) {
        if (strcmp(g_best_entries[i].key, key) == 0) {
            return &g_best_entries[i];
        }
    }
    return NULL;
}

static bool best_store_mount(void)
{
    if (g_best_mounted) {
        return true;
    }

    if (!sd_init_driver()) {
        printf("best_store: sd_init_driver failed\n");
        return false;
    }

    FRESULT fr = f_mount(&g_best_fs, "", 1);
    if (fr != FR_OK) {
        printf("best_store: f_mount error: %s (%d)\n", FRESULT_str(fr), fr);
        return false;
    }

    g_best_mounted = true;
    return true;
}

static void best_store_set_entry(const char *key, bool has_value, uint32_t value)
{
    best_store_entry_t *entry = best_store_find_entry(key);

    if (entry == NULL) {
        if (g_best_entry_count >= BEST_STORE_MAX_ENTRIES) {
            return;
        }
        entry = &g_best_entries[g_best_entry_count++];
        snprintf(entry->key, sizeof(entry->key), "%s", key);
    }

    entry->has_value = has_value;
    entry->value = value;
}

static bool best_store_load_cache(void)
{
    FIL file;
    FRESULT fr;
    char line[48];

    if (g_best_loaded) {
        return true;
    }
    if (!best_store_mount()) {
        return false;
    }

    g_best_entry_count = 0;
    fr = f_open(&file, BEST_STORE_FILENAME, FA_READ);
    if (fr == FR_NO_FILE || fr == FR_NO_PATH) {
        g_best_loaded = true;
        return true;
    }
    if (fr != FR_OK) {
        printf("best_store: f_open(%s) error: %s (%d)\n",
               BEST_STORE_FILENAME,
               FRESULT_str(fr),
               fr);
        return false;
    }

    while (f_gets(line, sizeof(line), &file) != NULL) {
        char key[BEST_STORE_KEY_SIZE];
        unsigned int has_value = 0;
        unsigned long parsed_value = 0;

        if (sscanf(line, "%15s %u %lu", key, &has_value, &parsed_value) == 3) {
            best_store_set_entry(key, has_value != 0u, (uint32_t)parsed_value);
        }
    }

    fr = f_close(&file);
    if (fr != FR_OK) {
        printf("best_store: f_close(%s) error: %s (%d)\n",
               BEST_STORE_FILENAME,
               FRESULT_str(fr),
               fr);
        return false;
    }

    g_best_loaded = true;
    return true;
}

static bool best_store_write_cache(void)
{
    FIL file;
    FRESULT fr;

    if (!best_store_load_cache()) {
        return false;
    }

    fr = f_open(&file, BEST_STORE_FILENAME, FA_CREATE_ALWAYS | FA_WRITE);
    if (fr != FR_OK) {
        printf("best_store: f_open(%s) error: %s (%d)\n",
               BEST_STORE_FILENAME,
               FRESULT_str(fr),
               fr);
        return false;
    }

    for (size_t i = 0; i < g_best_entry_count; ++i) {
        char line[48];
        UINT written = 0;
        int len = snprintf(line,
                           sizeof(line),
                           "%s %u %lu\n",
                           g_best_entries[i].key,
                           g_best_entries[i].has_value ? 1u : 0u,
                           (unsigned long)g_best_entries[i].value);

        if (len < 0 || len >= (int)sizeof(line)) {
            continue;
        }

        fr = f_write(&file, line, (UINT)len, &written);
        if (fr != FR_OK || written != (UINT)len) {
            printf("best_store: f_write(%s) error: %s (%d)\n",
                   BEST_STORE_FILENAME,
                   FRESULT_str(fr),
                   fr);
            f_close(&file);
            return false;
        }
    }

    fr = f_sync(&file);
    if (fr != FR_OK) {
        printf("best_store: f_sync(%s) error: %s (%d)\n",
               BEST_STORE_FILENAME,
               FRESULT_str(fr),
               fr);
        f_close(&file);
        return false;
    }

    fr = f_close(&file);
    if (fr != FR_OK) {
        printf("best_store: f_close(%s) error: %s (%d)\n",
               BEST_STORE_FILENAME,
               FRESULT_str(fr),
               fr);
        return false;
    }

    return true;
}

bool best_store_load_u32(const char *key, uint32_t *value, bool *has_value)
{
    best_store_entry_t *entry;

    if (value != NULL) {
        *value = 0u;
    }
    if (has_value != NULL) {
        *has_value = false;
    }

    if (!best_store_load_cache()) {
        return false;
    }

    entry = best_store_find_entry(key);
    if (entry == NULL) {
        return true;
    }

    if (value != NULL) {
        *value = entry->value;
    }
    if (has_value != NULL) {
        *has_value = entry->has_value;
    }
    return true;
}

bool best_store_save_u32(const char *key, uint32_t value, bool has_value)
{
    if (!best_store_load_cache()) {
        return false;
    }

    best_store_set_entry(key, has_value, value);
    return best_store_write_cache();
}
