#ifndef __USB_TASK_H__
#define __USB_TASK_H__

#include "../definitions.c"
#include "../utils/utils.c"
#include "../poc/poc.c"
#include <string.h>
#include "esp_timer.h"



static const char *USB_TASK_TAG = "USB_TASK";
char input_buffer[1024];
int input_len = 0;
bool hex_mode = false;
uint32_t last_input_time = 0;
#define INPUT_TIMEOUT_MS 500  // 0.5 seconds

// Helper function to check if a string contains only valid hex characters


// Helper function to reset input state
void reset_input_state(void) {
  input_len = 0;
  hex_mode = false;
  last_input_time = 0;
  memset(input_buffer, 0, sizeof(input_buffer));
}

void usb_passthrough(void) {
  // Always yield to other tasks to prevent watchdog timeout
  vTaskDelay(pdMS_TO_TICKS(10));

  // Check for input timeout - clear buffer if 0.5 seconds passed since last input
  if (input_len > 0 && last_input_time > 0) {
    uint32_t current_time = esp_timer_get_time() / 1000; // Convert to milliseconds
    if (current_time - last_input_time > INPUT_TIMEOUT_MS) {
      printf("\n[Timeout] Input buffer cleared after %dms of inactivity\n", INPUT_TIMEOUT_MS);
      reset_input_state();
    }
  }

  // Check if character is available (non-blocking)
  int c = getchar();

  if (c == EOF) {
    // No character available, continue to next iteration
    return;
  }

  if (c == '\n' || c == '\r') {
    // Always reset buffer state to prevent newline flood
    if (input_len > 0) {
      // Null terminate the buffer
      input_buffer[input_len] = '\0';

      // Check for special commands first
      if (strcmp(input_buffer, "help") == 0) {
        printf("\n=== UART Passthrough Commands ===\n");
        printf("Text commands: Type any text and press Enter\n");
        printf("Hex commands: Type hex values (0-9, A-F) and press Enter\n");
        printf("Examples:\n");
        printf("  Text: hello world\n");
        printf("  Hex:  48656C6C6F (Hello)\n");
        printf("  Hex:  FF 00 AA BB (with spaces)\n");
        printf("  Hex:  FF00AABB (without spaces)\n");
        printf("Special commands:\n");
        printf("  help - Show this help\n");
        printf("  status - Show UART status\n");
        printf("  hex - Toggle hex mode\n");
        printf("  text - Toggle text mode\n");
        printf("================================\n\n");
      } else if (strcmp(input_buffer, "hex") == 0) {
        hex_mode = true;
        printf("Hex mode enabled. All input will be treated as hex.\n");
      } else if (strcmp(input_buffer, "text") == 0) {
        hex_mode = false;
        printf("Text mode enabled. All input will be treated as text.\n");
      } else if (strcmp(input_buffer, "establish_handshake") == 0) {
        bool result = run_handshake(0);
        printf("Establishing handshake... %s\n", result ? "true" : "false");   
      } else if (strcmp(input_buffer, "get_bl_version") == 0) {
        bool result = get_bl_version();
        printf("Getting BL version... %s\n", result ? "true" : "false");
      } else if (strstr(input_buffer, "establish_handshake=") != NULL) {
        int times = atoi(strstr(input_buffer, "establish_handshake=") + strlen("establish_handshake="));
        bool result = run_handshake(times);
        printf("Establishing handshake... %s\n", result ? "true" : "false");
      } else if (strcmp(input_buffer, "install_stager") == 0) {
        install_stager_to_iram();
        printf("Installing stager to IRAM...\n");
      } else if (strcmp(input_buffer, "restart_plc") == 0) {
        restart_plc();
        printf("Restarting PLC...\n");
      } else if (strcmp(input_buffer, "power_off_plc") == 0) {
        relay_power_off();
        printf("Powering off PLC...\n");
      } else if (strcmp(input_buffer, "power_on_plc") == 0) {
        relay_power_on();
        printf("Powering on PLC...\n");
      } else if (strcmp(input_buffer, "status") == 0) {
        printf("UART Status:\n");
        printf("  TX Pin: GPIO %d\n", UART_TX_PIN);
        printf("  RX Pin: GPIO %d\n", UART_RX_PIN);
        printf("  Baud Rate: %d\n", UART_BAUD_RATE);
        printf("  Relay State: %s\n", relay_get_state() ? "ON" : "OFF");
        printf("  Parity: Even\n");
        printf("  Free heap: %" PRIu32 " bytes\n", esp_get_free_heap_size());
        printf("  Input mode: %s\n", hex_mode ? "HEX" : "TEXT");
      } else {
        // Process input based on mode or auto-detect
        bool should_treat_as_hex = hex_mode || is_valid_hex_string(input_buffer, input_len);
        
        if (should_treat_as_hex) {
          // Clean the hex string (remove spaces, convert to uppercase)
          clean_hex_string(input_buffer, &input_len);
          
          // Validate hex string length (must be even for proper byte conversion)
          if (input_len % 2 != 0) {
            printf("Error: Hex string length must be even (each byte = 2 hex chars)\n");
            printf("Input: %s (length: %d)\n", input_buffer, input_len);
          } else {
            // Convert hex to binary
            uint8_t binary_data[512]; // Increased size for larger inputs
            int binary_len = hex_to_binary(input_buffer, input_len, binary_data, sizeof(binary_data));

            if (binary_len > 0) {
              printf("[USB->UART] HEX: ");
              for (int i = 0; i < binary_len; i++) {
                printf("%02X ", binary_data[i]);
              }
              printf("(%d bytes)\n", binary_len);

              // Send binary data to UART
              uart_write_bytes(UART_NUM, binary_data, binary_len);
            } else {
              printf("Error: Failed to convert hex string to binary\n");
              printf("Input: %s\n", input_buffer);
            }
          }
        } else {
          // Send as text
          printf("[USB->UART] TEXT: %s\n", input_buffer);
          uart_write_bytes(UART_NUM, input_buffer, input_len);
        }
      }
    }
    // Always reset buffer state, even for empty commands
    reset_input_state();
    return;
  } else if (c >= 32 && c <= 126) {
    // Printable character - add with buffer overflow protection
    if (input_len < sizeof(input_buffer) - 1) {
      input_buffer[input_len++] = c;
      last_input_time = esp_timer_get_time() / 1000; // Update timestamp
      // putchar(c);  // Echo character
      // fflush(stdout);
    } else {
      // Buffer overflow - reset and show warning
      printf("\nWarning: Input buffer overflow, resetting...\n");
      reset_input_state();
    }
  } else if (c == 8 || c == 127) {
    // Backspace
    if (input_len > 0) {
      input_len--;
      last_input_time = esp_timer_get_time() / 1000; // Update timestamp
      // printf("\b \b");  // Backspace, space, backspace
      // fflush(stdout);
    }
  } else if (c == 3) {
    // Ctrl+C - reset input buffer
    printf("\n^C - Input cancelled, resetting buffer...\n");
    reset_input_state();
  } else {
    // Handle other control characters
    if (c < 32) {
      printf("\nWarning: Received control character 0x%02X, ignoring...\n", c);
    }
  }
}
/**
 * @brief USB task - handles data from USB serial and forwards to UART
 */
static void usb_task(void *pvParameters) {

  ESP_LOGI(USB_TASK_TAG, "USB task started");
  while (1) {
    usb_passthrough();
  }
}

#endif // __USB_TASK_H__