#ifndef __TASKER_RUNNER_H__
#define __TASKER_RUNNER_H__

#include "../definitions.c"

#include "uart_task.c"
#include "usb_task.c"
#include "wifi_task.c"

static const char *TASKS_RUNNER = "TASKER_RUNNER";

void run_tasks(void) {
    BaseType_t ret1 = xTaskCreate(uart_task, "uart_task", UART_TASK_STACK_SIZE, NULL, UART_TASK_PRIORITY, NULL);
    if (ret1 != pdPASS) {
        ESP_LOGE(TASKS_RUNNER, "Failed to create uart_task");
        return;
    }
    BaseType_t ret2 = xTaskCreate(usb_task, "usb_task", USB_TASK_STACK_SIZE, NULL, USB_TASK_PRIORITY, NULL);
    if (ret2 != pdPASS) {
        ESP_LOGE(TASKS_RUNNER, "Failed to create usb_task");
        return;
    }
    BaseType_t ret3 = xTaskCreate(wifi_task, "wifi_task", WIFI_TASK_STACK_SIZE, NULL, WIFI_TASK_PRIORITY, NULL);
    if (ret3 != pdPASS) {
        ESP_LOGE(TASKS_RUNNER, "Failed to create wifi_task");
        return;
    }
}

#endif // __TASKER_RUNNER_H__