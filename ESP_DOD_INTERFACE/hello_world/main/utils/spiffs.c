#ifndef __SPIFFS_H__
#define __SPIFFS_H__

#include "../definitions.c"
#include "../utils/utils.c"
#include "esp_spiffs.h"
#include <dirent.h>

static const char *SPIFFS_TAG = "SPIFFS";
static bool spiffs_initialized = false;

bool init_spiffs(void) {
    if (spiffs_initialized) {
        return true;
    }
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = true
    };
    
    esp_err_t err = esp_vfs_spiffs_register(&conf);
    if (err != ESP_OK) {
        ESP_LOGE(SPIFFS_TAG, "Failed to register SPIFFS");
        return false;
    }
    spiffs_initialized = true;
    return true;
}

void print_spiffs_files(void) {
    init_spiffs();
    DIR *dir = opendir("/spiffs");
    if (dir == NULL) {
        ESP_LOGE(SPIFFS_TAG, "Failed to open directory");
        return;
    }
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        ESP_LOGI(SPIFFS_TAG, "File: %s", entry->d_name);
    }
    closedir(dir);
}

int get_spiffs_file_size(const char *file_name) {
    init_spiffs();
    printf("File name: %s\n", file_name);
    FILE *file = fopen(file_name, "rb");
    if (file == NULL) {
        ESP_LOGE(SPIFFS_TAG, "Failed to open file");
        return -1;
    }
    fseek(file, 0, SEEK_END);
    int file_size = ftell(file);
    fclose(file);
    return file_size;
}


void print_spiffs_file_contents(const char *file_name) {
    init_spiffs();
    ESP_LOGI(SPIFFS_TAG, "Printing file contents: %s", file_name);
    FILE *file = fopen(file_name, "rb");
    if (file == NULL) {
        ESP_LOGE(SPIFFS_TAG, "Failed to open file");
        return;
    }

    char buffer[1024];
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        for (size_t i = 0; i < bytes_read; ++i) {
            printf("%02X ", (unsigned char)buffer[i]);
            // Optional: print a newline every 16 bytes for readability
            if ((i + 1) % 16 == 0) {
                printf("\n");
            }
        }
    }
    printf("\n");
    fclose(file);
}   


// Loads a file from SPIFFS into buffer and sets the actual size read in *out_size.
// Returns true on success, false on failure.
bool load_spiffs_file_to_memory(const char *file_name, uint8_t *buffer, size_t buffer_size, size_t *out_size) {
    init_spiffs();
    if (out_size) *out_size = 0;
    FILE *file = fopen(file_name, "rb");
    if (file == NULL) {
        ESP_LOGE(SPIFFS_TAG, "Failed to open file");
        return false;
    }
    size_t bytes_read = fread(buffer, 1, buffer_size, file);
    if (bytes_read == 0 && ferror(file)) {
        ESP_LOGE(SPIFFS_TAG, "Failed to read file");
        fclose(file);
        return false;
    }
    fclose(file);
    if (out_size) *out_size = bytes_read;
    return true;
}

#endif // __SPIFFS_H__