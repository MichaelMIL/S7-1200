#ifndef __RELAY_CONTROL_H__
#define __RELAY_CONTROL_H__

#include "../definitions.c"
#include "../utils/utils.c"

static const char *RELAY_CONTROL_TAG = "RELAY_CONTROL";


// Global relay state
static bool relay_state = false;




// Relay control functions
static void relay_set_state(bool state)
{
    relay_state = state;
    int gpio_level = RELAY_ACTIVE_HIGH ? (state ? 1 : 0) : (state ? 0 : 1);
    gpio_set_level(RELAY_GPIO_PIN, gpio_level);
    ESP_LOGI(RELAY_CONTROL_TAG, "Relay %s", state ? "ON" : "OFF");
}

bool relay_get_state(void) {
    return relay_state;
}

void relay_power_off(void) {
    printf("Relay power off\n");
    relay_set_state(false);
}

void relay_power_on(void) {
    printf("Relay power on\n");
    relay_set_state(true);
}



static void relay_init(void)
{
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << RELAY_GPIO_PIN),
        .pull_down_en = 0,
        .pull_up_en = 0,
    };
    gpio_config(&io_conf);
    
    // Initialize relay to OFF state
    relay_set_state(false);
    ESP_LOGI(RELAY_CONTROL_TAG, "Relay initialized on GPIO %d (Active %s)", RELAY_GPIO_PIN, RELAY_ACTIVE_HIGH ? "HIGH" : "LOW");
}




#endif // __RELAY_CONTROL_H__