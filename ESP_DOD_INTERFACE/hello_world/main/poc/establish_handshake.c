#ifndef __ESTABLISH_HANDSHAKE_H__
#define __ESTABLISH_HANDSHAKE_H__

#include "../definitions.c"
#include "../utils/utils.c"
#include "commands.c"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

// to establish HS with the PLC, we need to do the following:
// 1. power off the PLC
// 2. wait for 1 second
// 3. power on the PLC
// 4. start sending this handshake message: "AAAAMFGT1"
// 5. wait for 0.3 secounds for the PLC to respond
// 6. if it didn't respond, send the message again (for 5 times)
// 7. if it failed for 5 times, start from step 1
// 8. if it responded, check if the response is "\x05-CPU\xE6"
// 9. if not, start from step 1
// 10. if it is return success


bool establish_handshake(void) {
    restart_plc();
    uint8_t response_buffer[10];
    int response_len = 0;
    bool result = send_command_string_and_validate_response(CPU_CMND_STRING, 5, response_buffer, &response_len, 300, false);
    if (!result) {
      printf("Error: Failed to establish handshake\n");
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
    if (response_len == 4 && response_buffer[0] == '-' &&
           response_buffer[1] == 'C' && response_buffer[2] == 'P' &&
           response_buffer[3] == 'U' ) {
      return true;
    }
    return false;

}

bool run_handshake(int times) {

  if (times == 0) {
    printf("Running handshake forever...(until the PLC is connected)\n");
    while (1) {
      if (establish_handshake()) {
        return true;
      }
    }
  }
  int result = false;
  printf("Running handshake %d times...\n", times);

  for (int i = 0; i < times; i++) {
    if (establish_handshake()) {
      result = true;
      break;
    }
  }
  printf("Handshake result: %s\n", result ? "true" : "false");
  return result;
}

#endif // __ESTABLISH_HANDSHAKE_H__