#ifndef __CHECKSUM_H__
#define __CHECKSUM_H__

#include "../definitions.c"
#include "../utils/utils.c"



/**
 * @brief Calculate checksum for data packet
 * @param data Pointer to data bytes (excluding length and checksum)
 * @param len Number of data bytes
 * @param length_byte The length byte value
 * @return Checksum byte (first byte of 32-bit little-endian negative sum)
 */
 uint8_t calculate_checksum(const uint8_t* data, int len, uint8_t length_byte) {
    int sum = length_byte; // Start with length byte (like Python: incoming[:incoming[0]])
    
    // Sum all data byte values
    for (int i = 0; i < len; i++) {
        sum += data[i];
    }
    
    // Return first byte of 32-bit little-endian negative sum
    // This matches: struct.pack("<i", -sum)[0] in Python
    int32_t negative_sum = -sum;
    return (uint8_t)(negative_sum & 0xFF);
}

/**
 * @brief Create a complete packet with length, data, and checksum
 * @param data Input data bytes
 * @param data_len Length of input data
 * @param packet Output buffer for complete packet
 * @param max_packet_len Maximum size of output buffer
 * @return Total packet length, or -1 on error
 */
int create_packet(const uint8_t* data, int data_len, uint8_t* packet, int max_packet_len) {
    // Check if we have enough space: 1 byte (length) + data + 1 byte (checksum)
    if (max_packet_len < data_len + 2) {
        return -1;
    }
    
    // Set length byte (includes data + checksum)
    packet[0] = (uint8_t)(data_len + 1);
    
    // Copy data
    for (int i = 0; i < data_len; i++) {
        packet[i + 1] = data[i];
    }
    
    // Calculate and add checksum
    uint8_t checksum = calculate_checksum(data, data_len, packet[0]);
    packet[data_len + 1] = checksum;
    
    return data_len + 2; // Total packet length
}

/**
 * @brief Create a complete packet from a string
 * @param str Input string (null-terminated)
 * @param packet Output buffer for complete packet
 * @param max_packet_len Maximum size of output buffer
 * @return Total packet length, or -1 on error
 */
int create_packet_from_string(const char* str, uint8_t* packet, int max_packet_len) {
    if (str == NULL || packet == NULL) {
        return -1;
    }
    
    int str_len = strlen(str);
    return create_packet((const uint8_t*)str, str_len, packet, max_packet_len);
}

/**
 * @brief Verify packet checksum
 * @param packet Complete packet (length + data + checksum)
 * @param packet_len Total packet length
 * @return true if checksum is valid, false otherwise
 */
bool verify_packet_checksum(const uint8_t* packet, int packet_len) {
    if (packet_len < 3) { // Minimum: length + 1 data byte + checksum
        return false;
    }
    
    uint8_t len = packet[0];
    if (packet_len != len + 1) { // length + data + checksum (len includes checksum)
        return false;
    }
    
    // Calculate expected checksum
    int data_len = len - 1; // len includes checksum, so data_len = len - 1
    uint8_t expected_checksum = calculate_checksum(&packet[1], data_len, packet[0]);
    uint8_t actual_checksum = packet[len];
    
    return expected_checksum == actual_checksum;
}


void test_checksum(void) {
    // create packet with data "01 02 03 04 05"
    uint8_t data[4] = {0x00};
    uint8_t packet[10];
    int packet_len = create_packet(data, 1, packet, 10);
    printf("Packet: ");
    for (int i = 0; i < packet_len; i++) {
      printf("%02X ", packet[i]);
    }
    printf("\n");

    // verify packet checksum
    bool result = verify_packet_checksum(packet, packet_len);
    printf("Packet checksum: %s\n", result ? "true" : "false");
    data[0] = '-';
    data[1] = 'C';
    data[2] = 'P';
    data[3] = 'U';
    packet_len = create_packet(data, 4, packet, 10);
    printf("Packet: ");
    for (int i = 0; i < packet_len; i++) {
      printf("%02X ", packet[i]);
    }
    printf("\n");
    result = verify_packet_checksum(packet, packet_len);
    printf("Packet checksum: %s\n", result ? "true" : "false");
    
    // Test create_packet_from_string
    printf("\nTesting create_packet_from_string:\n");
    const char* test_string = "Hello World";
    packet_len = create_packet_from_string(test_string, packet, 20);
    printf("String: '%s'\n", test_string);
    printf("Packet: ");
    for (int i = 0; i < packet_len; i++) {
        printf("%02X ", packet[i]);
    }
    printf("\n");
    result = verify_packet_checksum(packet, packet_len);
    printf("Packet checksum: %s\n", result ? "true" : "false");
}

#endif // __CHECKSUM_H__