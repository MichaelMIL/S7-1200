#ifndef __UART_TASK_H__
#define __UART_TASK_H__

#include "../definitions.c"
#include "../utils/utils.c"

static const char *UART_TASK_TAG = "UART_TASK";
void print_initial_data(void) {

  ESP_LOGI(UART_TASK_TAG, "USB task started");
  printf("\n=== UART Serial Passthrough for ESP32-S3 ===\n");

  // Print chip information
  esp_chip_info_t chip_info;
  esp_chip_info(&chip_info);
  printf("Chip: %s with %d CPU core(s), WiFi%s%s\n", CONFIG_IDF_TARGET,
         chip_info.cores, (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
         (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "");

  printf("UART Configuration:\n");
  printf("  TX Pin: GPIO %d (U1TXD)\n", UART_TX_PIN);
  printf("  RX Pin: GPIO %d (U1RXD)\n", UART_RX_PIN);
  printf("  Baud Rate: %d\n", UART_BAUD_RATE);
  printf("  UART Number: %d\n", UART_NUM);
  printf("\nType commands and press Enter to send to UART device\n");
  printf("Supports both text and hex format:\n");
  printf("  Text: hello world\n");
  printf("  Hex:  48656C6C6F (Hello)\n");
  printf("  Hex:  FF 00 AA BB (spaces allowed)\n");
  printf("Data received from UART will be displayed with [UART->USB] prefix\n");
  printf("====================================================\n\n");
}

/**
 * @brief Initialize UART for ESP32-S3
 */
static void uart_init(void) {
  uart_config_t uart_config = {
      .baud_rate = UART_BAUD_RATE,
      .data_bits = UART_DATA_8_BITS,
      .parity = UART_PARITY_EVEN,
      .stop_bits = UART_STOP_BITS_1,
      .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
      .source_clk = UART_SCLK_DEFAULT,
  };

  // Install UART driver with larger buffer for ESP32-S3
  ESP_ERROR_CHECK(uart_driver_install(UART_NUM, UART_BUF_SIZE * 2,
                                      UART_BUF_SIZE * 2, 0, NULL, 0));

  // Configure UART parameters
  ESP_ERROR_CHECK(uart_param_config(UART_NUM, &uart_config));

  // Set UART pins (TX, RX, RTS, CTS)
  ESP_ERROR_CHECK(uart_set_pin(UART_NUM, UART_TX_PIN, UART_RX_PIN,
                               UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

  // Clear any existing data in UART buffer
  uart_flush(UART_NUM);

  ESP_LOGI(UART_TASK_TAG,
           "UART%d initialized for ESP32-S3 - TX: GPIO%d, RX: GPIO%d, Baud: %d",
           UART_NUM, UART_TX_PIN, UART_RX_PIN, UART_BAUD_RATE);
}
/**
 * @brief UART task - handles data from UART and forwards to USB
 */
static void uart_task(void *pvParameters) {
  // Initialize UART
  uart_init();
  uint8_t data[UART_BUF_SIZE];

  ESP_LOGI(UART_TASK_TAG, "UART task started");
  print_initial_data();
  while (1) {
    // if (!uart_task_allow_catching) {
    //   vTaskDelay(pdMS_TO_TICKS(10));
    //   continue;
    // }
    // Read data from UART with timeout to prevent blocking
    int len = uart_read_bytes(UART_NUM, data, UART_BUF_SIZE, pdMS_TO_TICKS(10));
    if (len > 0) {
      // Print received data to USB serial with prefix
      printf("[UART->USB] ");
      for (int i = 0; i < len; i++) {
        if (data[i] >= 32 && data[i] <= 126) {
          // Printable character
          printf("%c", data[i]);
        } else {
          // Non-printable character - show as hex
          printf("\\x%02X", data[i]);
        }
      }
      printf("\n");
      fflush(stdout);
    } else if (len == 0) {
      // No data available, yield to other tasks
      vTaskDelay(pdMS_TO_TICKS(10));
    }
  }
}

#endif // __UART_TASK_H__