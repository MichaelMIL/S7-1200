#ifndef __UTILS_H__
#define __UTILS_H__

#include "esp_log.h"
#include "esp_chip_info.h"
#include "../definitions.c"
#include "relay_control.c"
#include "checksum.c"
#include "spiffs.c"
#include <stdio.h>

static const char *UTILS_TAG = "UTILS";



void restart_plc(void) {
    relay_power_off();
    vTaskDelay(pdMS_TO_TICKS(1000));
    relay_power_on();
    vTaskDelay(pdMS_TO_TICKS(1000));
}

 /**
  * @brief Check if a string contains only hex characters (0-9, A-F, a-f, spaces)
  */
  static bool is_hex_string(const char* str, int len) {
    for (int i = 0; i < len; i++) {
        char c = str[i];
        if (!((c >= '0' && c <= '9') || 
              (c >= 'A' && c <= 'F') || 
              (c >= 'a' && c <= 'f') || 
              c == ' ')) {
            return false;
        }
    }
    return true;
}

/**
 * @brief Convert hex string to binary data
 * @param hex_str Input hex string
 * @param hex_len Length of hex string
 * @param binary_data Output buffer for binary data
 * @param max_binary_len Maximum size of output buffer
 * @return Number of bytes converted, or -1 on error
 */
static int hex_to_binary(const char* hex_str, int hex_len, uint8_t* binary_data, int max_binary_len) {
    int binary_len = 0;
    int i = 0;
    
    while (i < hex_len && binary_len < max_binary_len) {
        // Skip spaces
        while (i < hex_len && hex_str[i] == ' ') {
            i++;
        }
        
        if (i >= hex_len) break;
        
        // Read two hex characters
        char hex_byte[3] = {0};
        int j = 0;
        while (i < hex_len && j < 2 && hex_str[i] != ' ') {
            hex_byte[j++] = hex_str[i++];
        }
        
        if (j != 2) {
            return -1; // Invalid hex byte
        }
        
        // Convert hex string to byte
        char* endptr;
        unsigned long val = strtoul(hex_byte, &endptr, 16);
        if (*endptr != '\0' || val > 0xFF) {
            return -1; // Invalid hex value
        }
        
        binary_data[binary_len++] = (uint8_t)val;
    }
    
    return binary_len;
}
bool is_valid_hex_string(const char* str, int len) {
    if (len == 0) return false;
    
    for (int i = 0; i < len; i++) {
      char c = str[i];
      if (!((c >= '0' && c <= '9') || 
            (c >= 'A' && c <= 'F') || 
            (c >= 'a' && c <= 'f') || 
            c == ' ')) {
        return false;
      }
    }
    return true;
  }
  
  // Helper function to clean hex string (remove spaces, convert to uppercase)
  void clean_hex_string(char* str, int* len) {
    int write_pos = 0;
    for (int i = 0; i < *len; i++) {
      char c = str[i];
      if (c != ' ') {
        if (c >= 'a' && c <= 'f') {
          c = c - 'a' + 'A'; // Convert to uppercase
        }
        str[write_pos++] = c;
      }
    }
    *len = write_pos;
    str[write_pos] = '\0';
  }

#endif // __UTILS_H__