/*
 * UART Serial Passthrough for ESP32-S3
 * 
 * This application creates a UART passthrough that:
 * - Uses GPIO pins for UART RX/TX communication
 * - Forwards data between UART and USB serial
 * - Provides debugging output via USB serial
 * - Compatible with ESP32-S3 native USB capabilities
 */

 #include "definitions.c"
 #include "tasks/tasks_runner.c"
 #include "utils/utils.c"
 #include "poc/poc.c"
 static const char *TAG = "UART_PASSTHROUGH";
 




 /**
  * @brief Main application entry point for ESP32-S3
  */
 void app_main(void)
 {
     relay_init();
     init_spiffs();

     run_tasks();

     
     ESP_LOGI(TAG, "UART Serial Passthrough tasks created successfully");
     ESP_LOGI(TAG, "Ready for communication - TX: GPIO%d, RX: GPIO%d", UART_TX_PIN, UART_RX_PIN);

 }