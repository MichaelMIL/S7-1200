#ifndef __COMMANDS_H__
#define __COMMANDS_H__

#include "../definitions.c"
#include "../utils/utils.c"
#include <stdint.h>
#include "driver/uart.h"

#define CPU_CMND_STRING "AAAAMFGT1"
#define VER_CMND_STRING_WITH_CHECKSUM "02 00 FE"

uint8_t data[UART_BUF_SIZE];

bool send_command_string_and_validate_response(const char *command, int times,
                                               uint8_t *response_buffer,
                                               int *response_len,
                                               int timeout_ms,
                                               bool add_checksum) {
  uart_flush(UART_NUM);

  if (add_checksum) {
    uint8_t new_length =
        create_packet_from_string(command, data, UART_BUF_SIZE);
    printf("Packet: ");
    for (int i = 0; i < new_length; i++) {
      printf("%02X ", data[i]);
    }
    printf("\n");
    uart_write_bytes(UART_NUM, data, new_length);
  } else {
    uart_write_bytes(UART_NUM, command, strlen(command));
  }
  // uart_write_bytes(UART_NUM, command, strlen(command));
  for (int i = 0; i < times; i++) {
    int len =
        uart_read_bytes(UART_NUM, data, UART_BUF_SIZE, pdMS_TO_TICKS(300));
    if (len <= 0) {
      printf("No response (attempt %d)\n", i + 1);
      continue;
    }
    // Print the actual response received
    printf("Response received (%d bytes): ", len);
    for (int j = 0; j < len; j++) {
      if (data[j] >= 32 && data[j] <= 126) {
        printf("%c", data[j]);
      } else {
        printf("\\x%02X", data[j]);
      }
    }
    printf("\n");

    bool result =
        verify_packet_checksum(data, len); // bits 1 to 4 (data[1] to data[4])
    printf("Packet checksum: %s\n", result ? "true" : "false");
    if (result) {
      //     copy the data w/o the first and last byte
      if (response_buffer != NULL && response_len != NULL) {
        for (int j = 1; j < len - 1; j++) {
          response_buffer[j - 1] = data[j];
        }
        *response_len = len - 2;
      }
      return true;
    } else {
      printf("Response is not correct (attempt %d)\n", i + 1);
      continue;
    }
  }
  return false;
}


bool send_command_hex(const uint8_t *command_data, int command_len, bool add_checksum) {
  uart_flush(UART_NUM);

  if (add_checksum) {
    int new_length =
        create_packet(command_data, command_len, data, UART_BUF_SIZE);
    if (new_length == -1) {
      printf("Error: Failed to create packet\n");
      return false;
    }
    printf("Writing command: ");
    for (int i = 0; i < new_length; i++) {
      printf("%02X ", data[i]);
    }
    printf("\n");
    uart_write_bytes(UART_NUM, data, new_length);
  } else {
    printf("Writing command: ");
    for (int i = 0; i < command_len; i++) {
      printf("%02X ", command_data[i]);
    }
    printf("\n");
    uart_write_bytes(UART_NUM, command_data, command_len);
  }

  return true;
}

bool send_command_hex_and_validate_response(const uint8_t *command_data, int command_len,
                                           int times,
                                           uint8_t *response_buffer,
                                           int *response_len,
                                           int timeout_ms,
                                           bool add_checksum) {
  send_command_hex(command_data, command_len, add_checksum);
  for (int i = 0; i < times; i++) {
    int len =
        uart_read_bytes(UART_NUM, data, UART_BUF_SIZE, pdMS_TO_TICKS(timeout_ms));
    if (len <= 0) {
      printf("No response (attempt %d)\n", i + 1);
      continue;
    }
    // Print the actual response received
    printf("Response received (%d bytes): ", len);
    for (int j = 0; j < len; j++) {
      if (data[j] >= 32 && data[j] <= 126) {
        printf("%c", data[j]);
      } else {
        printf("\\x%02X", data[j]);
      }
    }
    printf("\n");

    bool result =
        verify_packet_checksum(data, len); // bits 1 to 4 (data[1] to data[4])
    printf("Packet checksum: %s\n", result ? "true" : "false");
    if (result) {
      //     copy the data w/o the first and last byte
      if (response_buffer != NULL && response_len != NULL) {
        for (int j = 1; j < len - 1; j++) {
          response_buffer[j - 1] = data[j];
        }
        *response_len = len - 2;
      }
      return true;
    } else {
      printf("Response is not correct (attempt %d)\n", i + 1);
      continue;
    }
  }
  return false;
}


#endif // __COMMANDS_H__