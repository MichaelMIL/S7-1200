#ifndef __DEFINITIONS_H__

#define __DEFINITIONS_H__
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// UART Configuration for ESP32-S3
#define UART_NUM UART_NUM_1
#define UART_TX_PIN GPIO_NUM_17 // GPIO 17 for TX (U1TXD)
#define UART_RX_PIN GPIO_NUM_18 // GPIO 18 for RX (U1RXD) - ESP32-S3 default
#define UART_BAUD_RATE 38400
#define UART_BUF_SIZE 1024

// Alternative GPIO pins for ESP32-S3 (uncomment to use different pins)
// #define UART_TX_PIN GPIO_NUM_21  // Alternative TX pin
// #define UART_RX_PIN GPIO_NUM_20  // Alternative RX pin

// Task priorities
#define UART_TASK_PRIORITY 5
#define UART_TASK_STACK_SIZE 8192
#define USB_TASK_PRIORITY 4
#define USB_TASK_STACK_SIZE 8192
#define WIFI_TASK_PRIORITY 3
#define WIFI_TASK_STACK_SIZE 16384
// Relay configuration
#define RELAY_GPIO_PIN 2    // GPIO pin for relay control
#define RELAY_ACTIVE_HIGH 0 // Set to 1 for active high, 0 for active low

// Queue for inter-task communication
static QueueHandle_t uart_queue;

bool uart_task_allow_catching = true;

#endif // __DEFINITIONS_H__