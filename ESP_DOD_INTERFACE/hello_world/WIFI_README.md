# ESP32-S7 Bootloader WiFi Interface

This document describes the WiFi functionality added to the ESP32-S7 Bootloader project.

## Overview

The WiFi task provides a web-based interface and REST API for:
- Connecting to the ESP32 via WiFi
- Executing commands (establish handshake, get bootloader version, etc.)
- Uploading text files to SPIFFS
- Viewing and managing files in SPIFFS
- System status monitoring

## WiFi Configuration

The ESP32 creates a WiFi access point with the following credentials:
- **SSID**: `ESP32-S7-Bootloader`
- **Password**: `s7bootloader123`
- **IP Address**: `192.168.4.1`
- **Port**: `80`

## Web Interface

Once connected to the WiFi network, open a web browser and navigate to:
```
http://192.168.4.1
```

The web interface provides:
- **System Status**: Shows WiFi connection, UART configuration, and system info
- **Command Execution**: Execute commands like `establish_handshake`, `get_bl_version`, etc.
- **File Upload**: Upload text files to SPIFFS
- **File Management**: List, view, and delete files in SPIFFS

## REST API Endpoints

### GET /api/status
Returns system status information.
```json
{
  "wifi_connected": true,
  "free_heap": 123456,
  "uart_tx": 17,
  "uart_rx": 18,
  "uart_baud": 38400
}
```

### POST /api/command
Execute a command on the ESP32.
```json
{
  "command": "establish_handshake"
}
```

### GET /api/files
List all files in SPIFFS.
```json
{
  "files": [
    {
      "name": "hello_world.txt",
      "size": 1024
    }
  ]
}
```

### POST /upload
Upload a file to SPIFFS (multipart/form-data).

### GET /api/file/{filename}
View file contents.

### DELETE /api/delete/{filename}
Delete a file from SPIFFS.

## Python Client

A Python client script is provided for command-line interaction:

```bash
# Get system status
python3 wifi_client.py status

# Execute a command
python3 wifi_client.py cmd establish_handshake

# List files
python3 wifi_client.py list

# Upload a file
python3 wifi_client.py upload myfile.txt

# View file contents
python3 wifi_client.py view myfile.txt

# Delete a file
python3 wifi_client.py delete myfile.txt
```

## Available Commands

The following commands can be executed via the web interface or API:

- `establish_handshake` - Establish handshake with PLC
- `get_bl_version` - Get bootloader version
- `install_stager` - Install stager to IRAM
- `restart_plc` - Restart the PLC
- `power_off_plc` - Power off the PLC
- `power_on_plc` - Power on the PLC
- `status` - Get detailed system status

## File Operations

### Supported File Types
- Text files (`.txt`)
- Any text-based files

### File Storage
- Files are stored in SPIFFS partition
- Maximum file size limited by available SPIFFS space
- Files are accessible via both web interface and API

## Security Considerations

- The WiFi access point uses WPA2-PSK encryption
- No additional authentication is required for web interface
- Consider changing default credentials for production use
- The interface is only accessible when connected to the ESP32's WiFi network

## Troubleshooting

### Cannot Connect to WiFi
1. Ensure the ESP32 is powered on and running
2. Check that the WiFi task is properly initialized
3. Verify the SSID and password are correct
4. Check if the ESP32 is in range

### Web Interface Not Loading
1. Verify you're connected to the correct WiFi network
2. Check the IP address (should be 192.168.4.1)
3. Ensure the HTTP server started successfully
4. Check ESP32 logs for error messages

### File Upload Issues
1. Ensure the file is a text file
2. Check available SPIFFS space
3. Verify the filename doesn't contain special characters
4. Check file permissions

## Building and Flashing

The WiFi functionality is automatically included when building the project:

```bash
cd tools/ESP_DOD/hello_world
idf.py build
idf.py flash
```

## Dependencies

The WiFi task requires the following ESP-IDF components:
- `esp_wifi`
- `esp_netif`
- `esp_event`
- `esp_http_server`
- `nvs_flash`
- `cjson`

These are automatically included in the CMakeLists.txt configuration.



