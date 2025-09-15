#ifndef __GET_BL_VERSION_H__
#define __GET_BL_VERSION_H__

#include "../definitions.c"
#include "../utils/utils.c"
#include "commands.c"

bool get_bl_version(void) {
  uint8_t response_buffer[10];
  int response_len = 0;
  uint8_t command[1] = {0x00};
  bool result = send_command_hex_and_validate_response(
      command, 1, 5, response_buffer, &response_len, 300, true);
  if (!result) {
    printf("Error: Failed to get BL version\n");
    return false;
  }
  printf("Response received (%d bytes): ", response_len);
  for (int j = 0; j < response_len; j++) {
    if (response_buffer[j] >= 32 && response_buffer[j] <= 126) {
      printf("%c", response_buffer[j]);
    } else {
      printf("\\x%02X", response_buffer[j]);
    }
  }
  printf("\n");
  // print response_buffer in pos 3,4,5,6
  printf("Version: %c %02X %02X %02X\n", response_buffer[2],
         response_buffer[3], response_buffer[4], response_buffer[5]);
  return true;
}

  // this function will return the BL version in a string
  // it will return the version in the format of "V0.0.0"
  char* return_bl_version(void) {
    uint8_t response_buffer[10];
    int response_len = 0;
    uint8_t command[1] = {0x00};
    bool result = send_command_hex_and_validate_response(
        command, 1, 5, response_buffer, &response_len, 300, true);
    if (!result) {
      printf("Error: Failed to get BL version\n");
      return NULL;
    }
    printf("Response received (%d bytes): ", response_len);
    for (int j = 0; j < response_len; j++) {
      if (response_buffer[j] >= 32 && response_buffer[j] <= 126) {
        printf("%c", response_buffer[j]);
      } else {
        printf("\\x%02X", response_buffer[j]);
      }
    }
    printf("\n");
    // print response_buffer in pos 3,4,5,6
    printf("Version: %c %02X %02X %02X\n", response_buffer[2],
          response_buffer[3], response_buffer[4], response_buffer[5]);
    
    // Format version as Vx.x.x
    char* version = (char*)malloc(11); // "VFF.FF.FF" + null terminator
    snprintf(version, 11, "V%02X.%02X.%02X", 
             response_buffer[3], response_buffer[4], response_buffer[5]);
    return version;
  }



#endif // __GET_BL_VERSION_H__