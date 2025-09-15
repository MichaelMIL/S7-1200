idf_component_register(SRCS "hello_world_main.c"
                       PRIV_REQUIRES spi_flash driver spiffs "esp_timer" "esp_wifi" "esp_netif" "esp_event" "esp_http_server" "nvs_flash" "json" "lwip"
                       INCLUDE_DIRS "tasks" "utils")
