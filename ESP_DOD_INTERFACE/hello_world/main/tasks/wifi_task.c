#ifndef __WIFI_TASK_H__
#define __WIFI_TASK_H__

#include "../definitions.c"
#include "../utils/utils.c"
#include "../utils/spiffs.c"
#include "../poc/establish_handshake.c"
#include "../poc/get_bl_version.c"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "freertos/idf_additions.h"
#include "nvs_flash.h"
#include "cJSON.h"
// #include "esp_ip.h"  // Not available in ESP-IDF v5.5
#include "esp_mac.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "driver/uart.h"
#include <stdbool.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

static const char *WIFI_TASK_TAG = "WIFI_TASK";

// WiFi configuration
#define WIFI_SSID "ESP32-S7-Bootloader"
#define WIFI_PASS "s7bootloader123"
#define WIFI_MAXIMUM_RETRY 5
#define WIFI_CHANNEL 1
#define WIFI_MAX_STA_CONN 4

// WiFi mode configuration - set to true for AP mode, false for STA mode
#define WIFI_AP_MODE false

// STA mode configuration (when connecting to existing network)
#define STA_SSID "Morties"
#define STA_PASS "ml123456"

// HTTP server configuration
#define HTTP_SERVER_PORT 80
#define HTTP_SERVER_MAX_URI_HANDLERS 10

// Global variables
static int s_retry_num = 0;
static httpd_handle_t server = NULL;
static bool wifi_connected = false;

// Forward declarations
static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
static void wifi_init_ap(void);
static void wifi_init_sta(void);
static esp_err_t root_handler(httpd_req_t *req);
static esp_err_t api_handler(httpd_req_t *req);
static esp_err_t upload_handler(httpd_req_t *req);
static esp_err_t files_handler(httpd_req_t *req);
static esp_err_t command_handler(httpd_req_t *req);
static esp_err_t send_file_handler(httpd_req_t *req);
static void start_webserver(void);
static void stop_webserver(void);

// File transmission functions
static bool establish_handshake_for_file_transmission(void);
static bool send_file_chunk(const char* chunk, int chunk_len,bool wait_for_response, const char* expected_response);
static bool send_file_via_uart(const char* filename);
static void format_hex_string(const char* input, char* output, int max_len);
static bool wait_for_uart_response(const char* expected_prefix, int timeout_ms);

// WiFi event handler
static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    if (WIFI_AP_MODE) {
        // AP mode events
        if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_START) {
            ESP_LOGI(WIFI_TASK_TAG, "WiFi AP started");
            wifi_connected = true;
            start_webserver();
        } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STOP) {
            ESP_LOGI(WIFI_TASK_TAG, "WiFi AP stopped");
            wifi_connected = false;
        } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED) {
            wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
            ESP_LOGI(WIFI_TASK_TAG, "station "MACSTR" join, AID=%d",
                     MAC2STR(event->mac), event->aid);
        } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED) {
            wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
            ESP_LOGI(WIFI_TASK_TAG, "station "MACSTR" leave, AID=%d",
                     MAC2STR(event->mac), event->aid);
        }
    } else {
        // STA mode events
        if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
            ESP_LOGI(WIFI_TASK_TAG, "WiFi STA started, initiating connection...");
            esp_err_t ret = esp_wifi_connect();
            if (ret != ESP_OK) {
                ESP_LOGE(WIFI_TASK_TAG, "Failed to initiate WiFi connection: %s", esp_err_to_name(ret));
            }
        } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED) {
            wifi_event_sta_connected_t* event = (wifi_event_sta_connected_t*) event_data;
            ESP_LOGI(WIFI_TASK_TAG, "WiFi STA connected to AP: "MACSTR" (SSID: %.*s, Channel: %d)",
                     MAC2STR(event->bssid), event->ssid_len, event->ssid, event->channel);
        } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
            wifi_event_sta_disconnected_t* event = (wifi_event_sta_disconnected_t*) event_data;
            ESP_LOGW(WIFI_TASK_TAG, "WiFi STA disconnected, reason: %d", event->reason);
            wifi_connected = false;
            if (s_retry_num < WIFI_MAXIMUM_RETRY) {
                ESP_LOGI(WIFI_TASK_TAG, "Retrying connection (%d/%d)...", s_retry_num + 1, WIFI_MAXIMUM_RETRY);
                esp_wifi_connect();
                s_retry_num++;
            } else {
                ESP_LOGE(WIFI_TASK_TAG, "Max retry attempts reached, giving up");
            }
        } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
            ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
            ESP_LOGI(WIFI_TASK_TAG, "Got IP: " IPSTR " (Gateway: " IPSTR ", Netmask: " IPSTR ")",
                     IP2STR(&event->ip_info.ip),
                     IP2STR(&event->ip_info.gw),
                     IP2STR(&event->ip_info.netmask));
            wifi_connected = true;
            s_retry_num = 0;
            ESP_LOGI(WIFI_TASK_TAG, "Starting web server...");
            start_webserver();
        }
    }
}

// Initialize WiFi in access point mode
static void wifi_init_ap(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_any_id));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = WIFI_SSID,
            .ssid_len = strlen(WIFI_SSID),
            .channel = WIFI_CHANNEL,
            .password = WIFI_PASS,
            .max_connection = WIFI_MAX_STA_CONN,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK,
            .pmf_cfg = {
                .capable = true,
                .required = false
            },
        },
    };
    
    if (strlen(WIFI_PASS) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(WIFI_TASK_TAG, "wifi_init_ap finished. SSID:%s password:%s",
             WIFI_SSID, WIFI_PASS);
}

// Initialize WiFi in station mode
static void wifi_init_sta(void)
{
    ESP_LOGI(WIFI_TASK_TAG, "Initializing WiFi in STA mode...");
    
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    if (sta_netif == NULL) {
        ESP_LOGE(WIFI_TASK_TAG, "Failed to create default WiFi STA interface");
        return;
    }

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_any_id));

    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = STA_SSID,
            .password = STA_PASS,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .capable = true,
                .required = false
            },
        },
    };

    ESP_LOGI(WIFI_TASK_TAG, "Configuring WiFi STA with SSID: '%s', Password: '%s'", 
             wifi_config.sta.ssid, wifi_config.sta.password);
    ESP_LOGI(WIFI_TASK_TAG, "Auth mode: %d, PMF capable: %s, PMF required: %s",
             wifi_config.sta.threshold.authmode,
             wifi_config.sta.pmf_cfg.capable ? "true" : "false",
             wifi_config.sta.pmf_cfg.required ? "true" : "false");

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(WIFI_TASK_TAG, "WiFi STA started, attempting to connect...");
    
    // Scan for available networks to help debug
    ESP_LOGI(WIFI_TASK_TAG, "Scanning for available networks...");
    wifi_scan_config_t scan_config = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = true,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
        .scan_time = {
            .active = {
                .min = 100,
                .max = 300
            }
        }
    };
    
    esp_err_t scan_ret = esp_wifi_scan_start(&scan_config, true);
    if (scan_ret == ESP_OK) {
        uint16_t ap_count = 0;
        esp_wifi_scan_get_ap_num(&ap_count);
        ESP_LOGI(WIFI_TASK_TAG, "Found %d access points", ap_count);
        
        if (ap_count > 0) {
            wifi_ap_record_t ap_records[ap_count];
            esp_wifi_scan_get_ap_records(&ap_count, ap_records);
            
            for (int i = 0; i < ap_count; i++) {
                ESP_LOGI(WIFI_TASK_TAG, "AP %d: SSID='%s', RSSI=%d, Auth=%d, Channel=%d",
                         i, ap_records[i].ssid, 
                         ap_records[i].rssi, ap_records[i].authmode, ap_records[i].primary);
            }
        }
    } else {
        ESP_LOGW(WIFI_TASK_TAG, "WiFi scan failed: %s", esp_err_to_name(scan_ret));
    }
}

// Root page handler - serves the main web interface from SPIFFS
static esp_err_t root_handler(httpd_req_t *req)
{
    ESP_LOGI(WIFI_TASK_TAG, "Serving root page from SPIFFS");
    
    // Initialize SPIFFS if not already done
    init_spiffs();
    
    // Open the HTML file from SPIFFS
    FILE *file = fopen("/spiffs/index.html", "r");
    if (file == NULL) {
        ESP_LOGE(WIFI_TASK_TAG, "Failed to open /spiffs/index.html");
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }
    
    // Set content type
    httpd_resp_set_type(req, "text/html");
    
    // Read and send file content in chunks
    char buffer[1024];
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        if (httpd_resp_send_chunk(req, buffer, bytes_read) != ESP_OK) {
            ESP_LOGE(WIFI_TASK_TAG, "Failed to send HTML chunk");
            fclose(file);
            return ESP_FAIL;
        }
    }
    
    // End response
    httpd_resp_send_chunk(req, NULL, 0);
    fclose(file);
    
    ESP_LOGI(WIFI_TASK_TAG, "HTML file served successfully");
    return ESP_OK;
}

// API status handler
static esp_err_t api_handler(httpd_req_t *req)
{
    if (strcmp(req->uri, "/api/status") == 0) {
        cJSON *json = cJSON_CreateObject();
        cJSON_AddBoolToObject(json, "wifi_connected", wifi_connected);
        cJSON_AddNumberToObject(json, "free_heap", esp_get_free_heap_size());
        cJSON_AddNumberToObject(json, "uart_tx", UART_TX_PIN);
        cJSON_AddNumberToObject(json, "uart_rx", UART_RX_PIN);
        cJSON_AddNumberToObject(json, "uart_baud", UART_BAUD_RATE);
        
        char *json_string = cJSON_Print(json);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, json_string, HTTPD_RESP_USE_STRLEN);
        
        free(json_string);
        cJSON_Delete(json);
        return ESP_OK;
    }
    return ESP_FAIL;
}

// Command execution handler
static esp_err_t command_handler(httpd_req_t *req)
{
    if (strcmp(req->uri, "/api/command") == 0) {
        char buf[512];
        int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
        if (ret <= 0) {
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }
        buf[ret] = '\0';
        
        cJSON *json = cJSON_Parse(buf);
        if (!json) {
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }
        
        cJSON *command_json = cJSON_GetObjectItem(json, "command");
        if (!command_json || !cJSON_IsString(command_json)) {
            cJSON_Delete(json);
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }
        
        const char *command = command_json->valuestring;
        char result[1024] = {0};
        
        // Execute commands similar to USB task
        if (strcmp(command, "establish_handshake") == 0) {
            bool success = run_handshake(1);
            snprintf(result, sizeof(result), "Establishing handshake... %s", success ? "SUCCESS" : "FAILED");
        } else if (strcmp(command, "get_bl_version") == 0) {
            char *version = return_bl_version();
            if (version) {
                snprintf(result, sizeof(result), "Bootloader Version: %s", version);
                free(version); // Free the allocated memory
            } else {
                snprintf(result, sizeof(result), "Failed to get bootloader version");
            }
        } 
        // else if (strcmp(command, "install_stager") == 0) {
        //     install_stager_to_iram();
        //     snprintf(result, sizeof(result), "Installing stager to IRAM... DONE");
        // }
        else if (strcmp(command, "restart_plc") == 0) {
            restart_plc();
            snprintf(result, sizeof(result), "Restarting PLC... DONE");
        } else if (strcmp(command, "power_off_plc") == 0) {
            relay_power_off();
            snprintf(result, sizeof(result), "Powering off PLC... DONE");
        } else if (strcmp(command, "power_on_plc") == 0) {
            relay_power_on();
            snprintf(result, sizeof(result), "Powering on PLC... DONE");
        } else if (strcmp(command, "status") == 0) {
            snprintf(result, sizeof(result), 
                "UART Status:\n"
                "  TX Pin: GPIO %d\n"
                "  RX Pin: GPIO %d\n"
                "  Baud Rate: %d\n"
                "  Relay State: %s\n"
                "  Free heap: %" PRIu32 " bytes",
                UART_TX_PIN, UART_RX_PIN, UART_BAUD_RATE,
                relay_get_state() ? "ON" : "OFF",
                esp_get_free_heap_size());
        } else {
            snprintf(result, sizeof(result), "Unknown command: %s", command);
        }
        
        cJSON_Delete(json);
        
        cJSON *response = cJSON_CreateObject();
        cJSON_AddStringToObject(response, "result", result);
        char *response_string = cJSON_Print(response);
        
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, response_string, HTTPD_RESP_USE_STRLEN);
        
        free(response_string);
        cJSON_Delete(response);
        return ESP_OK;
    }
    return ESP_FAIL;
}

// File upload handler - optimized for stack usage
static esp_err_t upload_handler(httpd_req_t *req)
{
    if (strcmp(req->uri, "/upload") == 0) {
        ESP_LOGI(WIFI_TASK_TAG, "File upload requested");
        
        // Initialize SPIFFS first
        init_spiffs();
        
        // Use smaller stack variables and dynamic allocation for large buffers
        char *buf = malloc(1024);
        if (!buf) {
            ESP_LOGE(WIFI_TASK_TAG, "Failed to allocate buffer");
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }
        
        int remaining = req->content_len;
        char filename[64] = "upload.txt"; // Smaller default filename
        char *boundary = NULL;
        
        // Find boundary in content type - make it more robust
        char content_type[128];
        esp_err_t header_ret = httpd_req_get_hdr_value_str(req, "Content-Type", content_type, sizeof(content_type));
        ESP_LOGI(WIFI_TASK_TAG, "Content-Type header result: %d", header_ret);
        
        if (header_ret == ESP_OK) {
            ESP_LOGI(WIFI_TASK_TAG, "Content-Type: %s", content_type);
            if ((boundary = strstr(content_type, "boundary="))) {
                boundary += 9; // Skip "boundary="
                // Remove any trailing characters (like semicolons)
                char *end = strchr(boundary, ';');
                if (end) *end = '\0';
                ESP_LOGI(WIFI_TASK_TAG, "Extracted boundary: '%s'", boundary);
            } else {
                ESP_LOGW(WIFI_TASK_TAG, "No boundary found in Content-Type, trying alternative approach");
                // Try to find boundary in the request body instead
                boundary = NULL;
            }
        } else {
            ESP_LOGW(WIFI_TASK_TAG, "Failed to get Content-Type header, trying without boundary");
            // Continue without boundary - some simple uploads might not need it
            boundary = NULL;
        }
        
        // Read headers to find filename
        int ret = httpd_req_recv(req, buf, 1023);
        if (ret <= 0) {
            ESP_LOGE(WIFI_TASK_TAG, "Failed to receive request data");
            free(buf);
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }
        buf[ret] = '\0';
        ESP_LOGI(WIFI_TASK_TAG, "Received %d bytes of headers", ret);
        
        // Extract filename from headers with better bounds checking
        ESP_LOGI(WIFI_TASK_TAG, "Looking for filename in buffer: %s", buf);
        
        char *filename_start = strstr(buf, "filename=\"");
        if (filename_start) {
            filename_start += 10; // Skip "filename="
            char *filename_end = strchr(filename_start, '"');
            if (filename_end && filename_end > filename_start) {
                int len = filename_end - filename_start;
                if (len > 0 && len < sizeof(filename) - 1) {
                    strncpy(filename, filename_start, len);
                    filename[len] = '\0';
                    ESP_LOGI(WIFI_TASK_TAG, "Extracted filename: %s", filename);
                } else {
                    ESP_LOGW(WIFI_TASK_TAG, "Filename too long, using default");
                }
            } else {
                ESP_LOGW(WIFI_TASK_TAG, "Invalid filename format, using default");
            }
        } else {
            // Try alternative filename extraction methods
            filename_start = strstr(buf, "name=\"file\"");
            if (filename_start) {
                ESP_LOGI(WIFI_TASK_TAG, "Found file field, using timestamp-based name");
                snprintf(filename, sizeof(filename), "upload_%lld.txt", (long long)(esp_timer_get_time() / 1000));
            } else {
                ESP_LOGW(WIFI_TASK_TAG, "No filename found in headers, using default");
            }
        }
        
        // Create full path with bounds checking
        char filepath[128];
        if (snprintf(filepath, sizeof(filepath), "/spiffs/%s", filename) >= sizeof(filepath)) {
            ESP_LOGE(WIFI_TASK_TAG, "Filepath too long");
            free(buf);
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }
        
        ESP_LOGI(WIFI_TASK_TAG, "Writing to file: %s", filepath);
        
        // Open file for writing
        FILE *file = fopen(filepath, "w");
        if (!file) {
            ESP_LOGE(WIFI_TASK_TAG, "Failed to open file for writing: %s", filepath);
            free(buf);
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }
        
        // Process multipart data properly
        int total_written = 0;
        bool in_file_content = false;
        
        // Try to extract boundary from the data itself if not found in headers
        if (!boundary) {
            // Look for boundary pattern in the first chunk
            char *boundary_start = strstr(buf, "------");
            if (boundary_start) {
                char *boundary_end = strstr(boundary_start, "\r\n");
                if (boundary_end) {
                    int boundary_len = boundary_end - boundary_start;
                    if (boundary_len > 6 && boundary_len < 64) {
                        // Extract the boundary (skip the leading dashes)
                        char extracted_boundary[64];
                        strncpy(extracted_boundary, boundary_start + 2, boundary_len - 2);
                        extracted_boundary[boundary_len - 2] = '\0';
                        boundary = extracted_boundary;
                        ESP_LOGI(WIFI_TASK_TAG, "Extracted boundary from data: '%s'", boundary);
                    }
                }
            }
        }
        
        if (boundary) {
            // Build the boundary strings - both start and end boundaries
            char boundary_start[128];
            char boundary_end[128];
            snprintf(boundary_start, sizeof(boundary_start), "--%s", boundary);
            snprintf(boundary_end, sizeof(boundary_end), "--%s--", boundary);
            ESP_LOGI(WIFI_TASK_TAG, "Looking for start boundary: %s", boundary_start);
            ESP_LOGI(WIFI_TASK_TAG, "Looking for end boundary: %s", boundary_end);
            
            // Find the start of file content (after headers)
            char *file_start = strstr(buf, "\r\n\r\n");
            if (file_start) {
                file_start += 4;
                in_file_content = true;
                ESP_LOGI(WIFI_TASK_TAG, "Found file content start");
            }
            
            // Process the first chunk
            if (in_file_content) {
                int header_len = file_start - buf;
                remaining -= header_len;
                int content_len = ret - header_len;
                
                // Check if this chunk contains the end boundary
                char *end_boundary = strstr(file_start, boundary_end);
                if (end_boundary) {
                    // Write only up to the boundary
                    int write_len = end_boundary - file_start;
                    if (write_len > 0) {
                        fwrite(file_start, 1, write_len, file);
                        total_written += write_len;
                        ESP_LOGI(WIFI_TASK_TAG, "Wrote %d bytes from first chunk (stopped at end boundary)", write_len);
                    }
                    remaining = 0; // We're done
                } else {
                    // Also check for the start boundary pattern (in case it's the end)
                    char *start_boundary = strstr(file_start, boundary_start);
                    if (start_boundary) {
                        // This might be the end boundary, write only up to it
                        int write_len = start_boundary - file_start;
                        if (write_len > 0) {
                            fwrite(file_start, 1, write_len, file);
                            total_written += write_len;
                            ESP_LOGI(WIFI_TASK_TAG, "Wrote %d bytes from first chunk (stopped at start boundary - likely end)", write_len);
                        }
                        remaining = 0; // We're done
                    } else {
                        // Check for any boundary-like pattern (dashes followed by alphanumeric)
                        char *any_boundary = strstr(file_start, "------");
                        if (any_boundary) {
                            // Found a boundary-like pattern, write only up to it
                            int write_len = any_boundary - file_start;
                            if (write_len > 0) {
                                fwrite(file_start, 1, write_len, file);
                                total_written += write_len;
                                ESP_LOGI(WIFI_TASK_TAG, "Wrote %d bytes from first chunk (stopped at boundary-like pattern)", write_len);
                            }
                            remaining = 0; // We're done
                        } else {
                            // Write the entire content
                            if (content_len > 0) {
                                fwrite(file_start, 1, content_len, file);
                                total_written += content_len;
                                ESP_LOGI(WIFI_TASK_TAG, "Wrote %d bytes from first chunk", content_len);
                            }
                        }
                    }
                }
            }
            
            // Process remaining chunks
            while (remaining > 0 && in_file_content) {
                int chunk_size = (remaining > 1024) ? 1024 : remaining;
                ret = httpd_req_recv(req, buf, chunk_size);
                if (ret <= 0) {
                    ESP_LOGW(WIFI_TASK_TAG, "End of data or error, ret=%d", ret);
                    break;
                }
                
                // Check for end boundary in this chunk
                char *end_boundary = strstr(buf, boundary_end);
                if (end_boundary) {
                    // Write only up to the boundary
                    int write_len = end_boundary - buf;
                    if (write_len > 0) {
                        fwrite(buf, 1, write_len, file);
                        total_written += write_len;
                        ESP_LOGI(WIFI_TASK_TAG, "Wrote %d bytes (stopped at end boundary)", write_len);
                    }
                    break; // We're done
                } else {
                    // Also check for the start boundary pattern (in case it's the end)
                    char *start_boundary = strstr(buf, boundary_start);
                    if (start_boundary) {
                        // This might be the end boundary, write only up to it
                        int write_len = start_boundary - buf;
                        if (write_len > 0) {
                            fwrite(buf, 1, write_len, file);
                            total_written += write_len;
                            ESP_LOGI(WIFI_TASK_TAG, "Wrote %d bytes (stopped at start boundary - likely end)", write_len);
                        }
                        break; // We're done
                    } else {
                        // Check for any boundary-like pattern (dashes followed by alphanumeric)
                        char *any_boundary = strstr(buf, "------");
                        if (any_boundary) {
                            // Found a boundary-like pattern, write only up to it
                            int write_len = any_boundary - buf;
                            if (write_len > 0) {
                                fwrite(buf, 1, write_len, file);
                                total_written += write_len;
                                ESP_LOGI(WIFI_TASK_TAG, "Wrote %d bytes (stopped at boundary-like pattern)", write_len);
                            }
                            break; // We're done
                        } else {
                            // Write the entire chunk
                            fwrite(buf, 1, ret, file);
                            total_written += ret;
                            remaining -= ret;
                            ESP_LOGI(WIFI_TASK_TAG, "Wrote %d bytes, %d remaining", ret, remaining);
                        }
                    }
                }
            }
        } else {
            // No boundary found, treat as simple file upload
            ESP_LOGI(WIFI_TASK_TAG, "No multipart boundary, treating as simple upload");
            
            // Skip to file content (after double CRLF)
            char *file_start = strstr(buf, "\r\n\r\n");
            if (file_start) {
                file_start += 4;
                int header_len = file_start - buf;
                remaining -= header_len;
                int content_len = ret - header_len;
                if (content_len > 0) {
                    fwrite(file_start, 1, content_len, file);
                    total_written += content_len;
                    ESP_LOGI(WIFI_TASK_TAG, "Wrote %d bytes from first chunk", content_len);
                }
            } else {
                // No headers, treat entire buffer as content
                fwrite(buf, 1, ret, file);
                total_written += ret;
                remaining -= ret;
                ESP_LOGI(WIFI_TASK_TAG, "Wrote %d bytes as file content", ret);
            }
            
            // Read remaining content
            while (remaining > 0) {
                int chunk_size = (remaining > 1024) ? 1024 : remaining;
                ret = httpd_req_recv(req, buf, chunk_size);
                if (ret <= 0) {
                    ESP_LOGW(WIFI_TASK_TAG, "End of data or error, ret=%d", ret);
                    break;
                }
                fwrite(buf, 1, ret, file);
                total_written += ret;
                remaining -= ret;
                ESP_LOGI(WIFI_TASK_TAG, "Wrote %d bytes, %d remaining", ret, remaining);
            }
        }
        
        fclose(file);
        free(buf);
        
        ESP_LOGI(WIFI_TASK_TAG, "File upload completed. Total bytes written: %d", total_written);
        
        char response[256];
        snprintf(response, sizeof(response), "File '%s' uploaded successfully to SPIFFS (%d bytes)", filename, total_written);
        httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    return ESP_FAIL;
}

// Files list handler
static esp_err_t files_handler(httpd_req_t *req)
{
    if (strcmp(req->uri, "/api/files") == 0) {
        ESP_LOGI(WIFI_TASK_TAG, "Files list requested");
        
        cJSON *json = cJSON_CreateObject();
        cJSON *files_array = cJSON_CreateArray();
        
        init_spiffs();
        ESP_LOGI(WIFI_TASK_TAG, "SPIFFS initialized, opening /spiffs directory");
        
        DIR *dir = opendir("/spiffs");
        if (dir != NULL) {
            ESP_LOGI(WIFI_TASK_TAG, "Directory opened successfully");
            struct dirent *entry;
            int file_count = 0;
            while ((entry = readdir(dir)) != NULL) {
                ESP_LOGI(WIFI_TASK_TAG, "Found entry: %s (type: %d)", entry->d_name, entry->d_type);
                if (entry->d_type == DT_REG) { // Regular file
                    file_count++;
                    cJSON *file_obj = cJSON_CreateObject();
                    cJSON_AddStringToObject(file_obj, "name", entry->d_name);
                    
                    char filepath[512];
                    snprintf(filepath, sizeof(filepath), "/spiffs/%s", entry->d_name);
                    int size = get_spiffs_file_size(filepath);
                    cJSON_AddNumberToObject(file_obj, "size", size);
                    
                    cJSON_AddItemToArray(files_array, file_obj);
                    ESP_LOGI(WIFI_TASK_TAG, "Added file to list: %s (%d bytes)", entry->d_name, size);
                }
            }
            closedir(dir);
            ESP_LOGI(WIFI_TASK_TAG, "Total files found: %d", file_count);
        } else {
            ESP_LOGE(WIFI_TASK_TAG, "Failed to open /spiffs directory");
        }
        
        cJSON_AddItemToObject(json, "files", files_array);
        char *json_string = cJSON_Print(json);
        
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, json_string, HTTPD_RESP_USE_STRLEN);
        
        free(json_string);
        cJSON_Delete(json);
        return ESP_OK;
    }
    return ESP_FAIL;
}

// File content handler
static esp_err_t file_content_handler(httpd_req_t *req)
{
    // Get filename from query parameter
    char filename[64];
    
    if (httpd_req_get_url_query_str(req, filename, sizeof(filename)) == ESP_OK) {
        ESP_LOGI(WIFI_TASK_TAG, "Query string: %s", filename);
        
        // Parse the filename from the query string
        char *file_param = strstr(filename, "file=");
        if (file_param) {
            file_param += 5; // Skip "file="
            // URL decode if needed (simple version)
            char *end = strchr(file_param, '&');
            if (end) *end = '\0';
            
            ESP_LOGI(WIFI_TASK_TAG, "Requested file: %s", file_param);
            
            char filepath[256];
            snprintf(filepath, sizeof(filepath), "/spiffs/%s", file_param);
            
            ESP_LOGI(WIFI_TASK_TAG, "Full file path: %s", filepath);
            
            // Initialize SPIFFS if not already done
            init_spiffs();
            
            FILE *file = fopen(filepath, "r");
            if (file) {
                ESP_LOGI(WIFI_TASK_TAG, "File opened successfully");
                
                // Set Content-Disposition header for download
                char content_disposition[128];
                snprintf(content_disposition, sizeof(content_disposition), "attachment; filename=\"%s\"", file_param);
                httpd_resp_set_hdr(req, "Content-Disposition", content_disposition);
                
                // Set appropriate content type based on file extension
                if (strstr(file_param, ".html") || strstr(file_param, ".htm")) {
                    httpd_resp_set_type(req, "text/html");
                } else if (strstr(file_param, ".txt")) {
                    httpd_resp_set_type(req, "text/plain");
                } else if (strstr(file_param, ".bin")) {
                    httpd_resp_set_type(req, "application/octet-stream");
                } else {
                    httpd_resp_set_type(req, "application/octet-stream");
                }
                
                char buffer[1024];
                size_t bytes_read;
                while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
                    httpd_resp_send_chunk(req, buffer, bytes_read);
                }
                httpd_resp_send_chunk(req, NULL, 0); // End response
                fclose(file);
                return ESP_OK;
            } else {
                ESP_LOGE(WIFI_TASK_TAG, "Failed to open file: %s", filepath);
            }
        } else {
            ESP_LOGE(WIFI_TASK_TAG, "No 'file' parameter found in query string");
        }
    } else {
        ESP_LOGE(WIFI_TASK_TAG, "Failed to get query string from URI: %s", req->uri);
    }
    
    httpd_resp_send_404(req);
    return ESP_FAIL;
}

// File delete handler
static esp_err_t file_delete_handler(httpd_req_t *req)
{
    // Get filename from query parameter
    char filename[64];
    
    if (httpd_req_get_url_query_str(req, filename, sizeof(filename)) == ESP_OK) {
        ESP_LOGI(WIFI_TASK_TAG, "Delete query string: %s", filename);
        
        // Parse the filename from the query string
        char *file_param = strstr(filename, "file=");
        if (file_param) {
            file_param += 5; // Skip "file="
            // URL decode if needed (simple version)
            char *end = strchr(file_param, '&');
            if (end) *end = '\0';
            
            ESP_LOGI(WIFI_TASK_TAG, "Delete file: %s", file_param);
            
            char filepath[256];
            snprintf(filepath, sizeof(filepath), "/spiffs/%s", file_param);
            
            ESP_LOGI(WIFI_TASK_TAG, "Delete file path: %s", filepath);
            
            // Initialize SPIFFS if not already done
            init_spiffs();
            
            if (remove(filepath) == 0) {
                ESP_LOGI(WIFI_TASK_TAG, "File deleted successfully: %s", filepath);
                char response[256];
                snprintf(response, sizeof(response), "File '%s' deleted successfully", file_param);
                httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
                return ESP_OK;
            } else {
                ESP_LOGE(WIFI_TASK_TAG, "Failed to delete file: %s", filepath);
            }
        } else {
            ESP_LOGE(WIFI_TASK_TAG, "No 'file' parameter found in delete query string");
        }
    } else {
        ESP_LOGE(WIFI_TASK_TAG, "Failed to get query string from delete URI: %s", req->uri);
    }
    
    httpd_resp_send_404(req);
    return ESP_FAIL;
}

// File transmission functions implementation

/**
 * @brief Format hex string by adding spaces every 2 characters (1 byte)
 */
static void format_hex_string(const char* input, char* output, int max_len) {
    int input_len = strlen(input);
    int output_pos = 0;
    
    for (int i = 0; i < input_len && output_pos < max_len - 2; i += 2) {
        if (i > 0) {
            output[output_pos++] = ' ';
        }
        if (i + 1 < input_len) {
            output[output_pos++] = input[i];
            output[output_pos++] = input[i + 1];
        } else {
            output[output_pos++] = input[i];
        }
    }
    output[output_pos] = '\0';
}

/**
 * @brief Wait for UART response with timeout
 */
static bool wait_for_uart_response(const char* expected_prefix, int timeout_ms) {
    uint8_t data[UART_BUF_SIZE];
    uint32_t start_time = esp_timer_get_time() / 1000; // Convert to milliseconds
    if (expected_prefix == NULL) {
        // Just wait for any response within the timeout
        while ((esp_timer_get_time() / 1000 - start_time) < timeout_ms) {
            int len = uart_read_bytes(UART_NUM, data, UART_BUF_SIZE, pdMS_TO_TICKS(10));
            printf("Received response: ");
            for (int i = 0; i < len; i++) {
                printf("%02X ", data[i]);
            }
            printf("(%d bytes)\n", len);
            if (len > 0) {
                ESP_LOGI(WIFI_TASK_TAG, "Received response (no prefix expected)");
                return true;
            }
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        return false;
    }
    while ((esp_timer_get_time() / 1000 - start_time) < timeout_ms) {
        int len = uart_read_bytes(UART_NUM, data, UART_BUF_SIZE, pdMS_TO_TICKS(10));
        printf("Received response: ");
        for (int i = 0; i < len; i++) {
          printf("%02X ", data[i]);
        }
        printf("(%d bytes)\n", len);
        if (len > 0) {
            data[len] = '\0'; // Null terminate for string comparison
            if (strstr((char*)data, expected_prefix) != NULL) {
                ESP_LOGI(WIFI_TASK_TAG, "Received expected response: %s", (char*)data);
                return true;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    return false;
}

/**
 * @brief Establish handshake for file transmission
 */
static bool establish_handshake_for_file_transmission(void) {
    ESP_LOGI(WIFI_TASK_TAG, "Establishing handshake for file transmission...");
    
    // Send handshake command
    const char* handshake_cmd = "establish_handshake\n";
    uart_write_bytes(UART_NUM, handshake_cmd, strlen(handshake_cmd));
    uart_flush(UART_NUM);
    
    // Wait for handshake response
    if (wait_for_uart_response("Establishing handshake... true", 3000)) {
        ESP_LOGI(WIFI_TASK_TAG, "Handshake established successfully");
        return true;
    } else {
        ESP_LOGE(WIFI_TASK_TAG, "Handshake failed or timeout");
        return false;
    }
}

/**
 * @brief Send a file chunk via UART and wait for response
 */
static bool send_file_chunk(const char* chunk, int chunk_len,bool wait_for_response, const char* expected_response) {
    ESP_LOGI(WIFI_TASK_TAG, "Sending chunk: %.*s", chunk_len, chunk);
    // Example chunk: 02 00 FE

    uint8_t binary_data[512]; // Increased size for larger inputs
    int binary_len = hex_to_binary(chunk, chunk_len, binary_data, sizeof(binary_data));
    if (binary_len > 0) {
        printf("[USB->UART] HEX: ");
        for (int i = 0; i < binary_len; i++) {
          printf("%02X ", binary_data[i]);
        }
        printf("(%d bytes)\n", binary_len);
    }

    // Send the chunk
    ESP_LOGI(WIFI_TASK_TAG, "Sending chunk via UART: %d bytes", binary_len);
    uart_write_bytes(UART_NUM, binary_data, binary_len);

    if (!wait_for_response) {
        return true;
    }
    // Wait for response
    if (wait_for_uart_response(expected_response, 3000)) {
        ESP_LOGI(WIFI_TASK_TAG, "Chunk sent successfully, received expected response");
        return true;
    } else {
        ESP_LOGE(WIFI_TASK_TAG, "Chunk transmission failed or timeout");
        return false;
    }
}

/**
 * @brief Send file via UART using the same logic as Python script
 */
static bool send_file_via_uart(const char* filename) {
    ESP_LOGI(WIFI_TASK_TAG, "Starting file transmission: %s", filename);
    
    uart_task_allow_catching = false;
    // Initialize SPIFFS
    init_spiffs();
    
    // Open file
    char filepath[256];
    snprintf(filepath, sizeof(filepath), "/spiffs/%s", filename);
    
    FILE *file = fopen(filepath, "r");
    if (!file) {
        ESP_LOGE(WIFI_TASK_TAG, "Failed to open file: %s", filepath);
        uart_task_allow_catching = true;
        return false;
    }
    
    char line[1024];
    int line_count = 0;
    bool success = true;
    
    // Read file line by line
    while (fgets(line, sizeof(line), file) != NULL && success) {
        printf("Processing line: %s", line);
        line_count++;
        
        // Remove spaces and newlines
        char clean_line[1024];
        int clean_pos = 0;
        for (int i = 0; line[i] != '\0' && clean_pos < sizeof(clean_line) - 1; i++) {
            if (line[i] != ' ' && line[i] != '\n' && line[i] != '\r') {
                clean_line[clean_pos++] = line[i];
            }
        }
        clean_line[clean_pos] = '\0';
        
        int line_len = strlen(clean_line);
        if (line_len == 0) continue; // Skip empty lines
        
        ESP_LOGI(WIFI_TASK_TAG, "Processing line %d, length: %d", line_count, line_len);
        
        const int BYTES = 16; // 16 bytes = 32 hex characters
        
        if (line_len > BYTES * 2) {
            // Split into 16-byte chunks
            for (int i = 0; i < line_len && success; i += BYTES * 2) {
                vTaskDelay(pdMS_TO_TICKS(300));

                char chunk[BYTES * 2 + 1];
                int chunk_len = (i + BYTES * 2 <= line_len) ? BYTES * 2 : (line_len - i);
                
                strncpy(chunk, clean_line + i, chunk_len);
                chunk[chunk_len] = '\0';
                
                // Format chunk with spaces
                char formatted_chunk[BYTES * 3 + 1];
                format_hex_string(chunk, formatted_chunk, sizeof(formatted_chunk));
                
                vTaskDelay(pdMS_TO_TICKS(30)); // 30ms delay between chunks
                
                if (!send_file_chunk(formatted_chunk, strlen(formatted_chunk), false, NULL)) {
                    ESP_LOGE(WIFI_TASK_TAG, "Failed to send chunk: %s", chunk);
                    success = false;
                    break;
                }
            }
            
            // Wait for final response after all chunks
            // if (success) {
            //     if (!wait_for_uart_response(NULL, 3000)) {
            //         ESP_LOGE(WIFI_TASK_TAG, "Failed to receive final response after chunks");
            //         success = false;
            //     }
            // }
        } else {
            // Send as single chunk
            vTaskDelay(pdMS_TO_TICKS(300));

            char formatted_line[line_len * 2 + 1];
            format_hex_string(clean_line, formatted_line, sizeof(formatted_line));
            
            vTaskDelay(pdMS_TO_TICKS(30)); // 30ms delay
            
            if (!send_file_chunk(formatted_line, strlen(formatted_line),false, NULL)) {
                ESP_LOGE(WIFI_TASK_TAG, "Failed to send line: %s", clean_line);
                success = false;
            }
        }
    }
    
    fclose(file);
    
    if (success) {
        ESP_LOGI(WIFI_TASK_TAG, "File transmission completed successfully");
    } else {
        ESP_LOGE(WIFI_TASK_TAG, "File transmission failed");
    }
    uart_task_allow_catching = true;
    
    return success;
}

// Send file handler
static esp_err_t send_file_handler(httpd_req_t *req)
{
    if (strcmp(req->uri, "/api/send_file") == 0) {
        char buf[512];
        int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
        if (ret <= 0) {
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }
        buf[ret] = '\0';
        
        cJSON *json = cJSON_Parse(buf);
        if (!json) {
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }
        
        cJSON *filename_json = cJSON_GetObjectItem(json, "filename");
        if (!filename_json || !cJSON_IsString(filename_json)) {
            cJSON_Delete(json);
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }
        
        const char *filename = filename_json->valuestring;
        ESP_LOGI(WIFI_TASK_TAG, "File transmission requested for: %s", filename);
        
        // Send file via UART
        bool success = send_file_via_uart(filename);
        
        cJSON_Delete(json);
        
        cJSON *response = cJSON_CreateObject();
        if (success) {
            cJSON_AddStringToObject(response, "result", "File sent successfully via UART");
            cJSON_AddBoolToObject(response, "success", true);
        } else {
            cJSON_AddStringToObject(response, "result", "Failed to send file via UART");
            cJSON_AddBoolToObject(response, "success", false);
        }
        
        char *response_string = cJSON_Print(response);
        
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, response_string, HTTPD_RESP_USE_STRLEN);
        
        free(response_string);
        cJSON_Delete(response);
        return ESP_OK;
    }
    return ESP_FAIL;
}

// Start web server
static void start_webserver(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = HTTP_SERVER_PORT;
    config.max_uri_handlers = HTTP_SERVER_MAX_URI_HANDLERS;
    config.stack_size = 8192; // Increase stack size for file uploads
    config.max_resp_headers = 8; // Increase header limit
    config.max_open_sockets = 7; // Increase socket limit

    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t root_uri = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = root_handler,
        };
        httpd_register_uri_handler(server, &root_uri);

        httpd_uri_t api_uri = {
            .uri = "/api/status",
            .method = HTTP_GET,
            .handler = api_handler,
        };
        httpd_register_uri_handler(server, &api_uri);

        httpd_uri_t command_uri = {
            .uri = "/api/command",
            .method = HTTP_POST,
            .handler = command_handler,
        };
        httpd_register_uri_handler(server, &command_uri);

        httpd_uri_t upload_uri = {
            .uri = "/upload",
            .method = HTTP_POST,
            .handler = upload_handler,
        };
        httpd_register_uri_handler(server, &upload_uri);

        httpd_uri_t files_uri = {
            .uri = "/api/files",
            .method = HTTP_GET,
            .handler = files_handler,
        };
        httpd_register_uri_handler(server, &files_uri);

        httpd_uri_t file_uri = {
            .uri = "/api/file",
            .method = HTTP_GET,
            .handler = file_content_handler,
        };
        httpd_register_uri_handler(server, &file_uri);

        httpd_uri_t delete_uri = {
            .uri = "/api/delete",
            .method = HTTP_DELETE,
            .handler = file_delete_handler,
        };
        httpd_register_uri_handler(server, &delete_uri);

        httpd_uri_t send_file_uri = {
            .uri = "/api/send_file",
            .method = HTTP_POST,
            .handler = send_file_handler,
        };
        httpd_register_uri_handler(server, &send_file_uri);

        ESP_LOGI(WIFI_TASK_TAG, "Web server started on port %d", HTTP_SERVER_PORT);
    } else {
        ESP_LOGE(WIFI_TASK_TAG, "Error starting web server");
    }
}

// Stop web server
static void stop_webserver(void)
{
    if (server) {
        httpd_stop(server);
        server = NULL;
        ESP_LOGI(WIFI_TASK_TAG, "Web server stopped");
    }
}

// WiFi task
static void wifi_task(void *pvParameters)
{
    ESP_LOGI(WIFI_TASK_TAG, "WiFi task started");
    
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize WiFi based on mode flag
    if (WIFI_AP_MODE) {
        ESP_LOGI(WIFI_TASK_TAG, "Starting WiFi in AP mode");
        wifi_init_ap();
    } else {
        ESP_LOGI(WIFI_TASK_TAG, "Starting WiFi in STA mode");
        wifi_init_sta();
    }
    
    // Keep task running
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

#endif // __WIFI_TASK_H__
