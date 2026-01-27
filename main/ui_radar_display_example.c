/*
 * ui_radar_display_example.c
 * Example integration of radar display with existing code
 *
 * This file shows how to integrate the radar display into your existing
 * main.c and mmwave.c code.
 */

#include "bsp/esp-bsp.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "humanRadarRD_03D.h"
#include "ui_radar_display.h"

static const char *TAG = "RadarDisplay";

// Global targets array that will be shared between tasks
static radar_target_t g_display_targets[RADAR_MAX_TARGETS];
static int g_target_count = 0;

/**
 * OPTION 1: Modified radar task that updates the display
 *
 * This is a modified version of vRadarTask from mmwave.c that includes
 * display updates.
 */
void vRadarTaskWithDisplay(void *pvParameters)
{
    radar_sensor_t radar;
    radar_target_t targets[RADAR_MAX_TARGETS];
    radar_target_t priorTargets[RADAR_MAX_TARGETS];

    // Initialize the radar sensor
    esp_err_t ret = radar_sensor_init(&radar, CONFIG_UART_PORT,
                                      CONFIG_UART_RX_GPIO,
                                      CONFIG_UART_TX_GPIO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Radar initialization failed: %s", esp_err_to_name(ret));
        vTaskDelete(NULL);
    }

    // Start UART communication
    ret = radar_sensor_begin(&radar, CONFIG_UART_SPEED_BPS);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start radar sensor");
        vTaskDelete(NULL);
    }

    // Configure radar
    radar_sensor_set_multi_target_mode(&radar, true);
    radar_sensor_set_retention_times(&radar, 10000, 500);

    ESP_LOGI(TAG, "Sensor is active, starting main loop.");

    // Main loop
    int target_count = 0;
    while (1) {
        if (radar_sensor_update(&radar)) {
            memcpy(priorTargets, targets, sizeof(targets));
            target_count = radar_sensor_get_targets(&radar, targets);

            // Update global targets for display
            memcpy(g_display_targets, targets, sizeof(targets));
            g_target_count = target_count;

            // Update the LVGL display
            bsp_display_lock(0);
            radar_display_update(g_display_targets, g_target_count);
            bsp_display_unlock();

            // Log if targets moved (optional)
            for (int idx = 0; idx < target_count; idx++) {
                if (targets[idx].detected) {
                    ESP_LOGI(TAG, "[%d] X:%.0f Y:%.0f D:%.0f A:%.1f S:%.0f",
                            idx,
                            targets[idx].x,
                            targets[idx].y,
                            targets[idx].distance,
                            targets[idx].angle,
                            targets[idx].speed);
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(500));  // Update every 500ms
    }
}

/**
 * OPTION 2: Separate display update task
 *
 * This approach uses a separate task that periodically updates the display
 * from a shared global targets array.
 */
void vDisplayUpdateTask(void *pvParameters)
{
    ESP_LOGI(TAG, "Display update task started");

    while (1) {
        // Update display with current target data
        bsp_display_lock(0);
        radar_display_update(g_display_targets, g_target_count);
        bsp_display_unlock();

        vTaskDelay(pdMS_TO_TICKS(250));  // Update display every 250ms
    }
}

/**
 * Modified app_main showing how to initialize the display
 *
 * This replaces the ui_skoona_page() call with the radar display
 */
void app_main_with_radar_display(void)
{
    lv_obj_t *screen = NULL;

    // ... (all your existing initialization code) ...

    // Initialize LVGL and display
    bsp_display_cfg_t cfg = {
        .lvgl_port_cfg = ESP_LVGL_PORT_INIT_CONFIG(),
        .buffer_size = BSP_LCD_H_RES * BSP_LCD_V_RES / 10,
        .double_buffer = false,
        .flags = {
            .buff_dma = true,
            .buff_spiram = false,
        },
    };
    lv_display_t *disp = bsp_display_start_with_config(&cfg);
    bsp_display_backlight_on();
    bsp_display_brightness_set(100);

    // Create radar display UI
    bsp_display_lock(0);
    screen = lv_disp_get_scr_act(disp);
    radar_display_create_ui(screen);
    bsp_display_unlock();

    // Start radar task (OPTION 1: with built-in display updates)
    xTaskCreatePinnedToCore(vRadarTaskWithDisplay,
                            "Radar Display",
                            4096,
                            NULL,
                            10,
                            NULL,
                            1);

    // OR use OPTION 2: Start both radar and separate display tasks
    /*
    xTaskCreatePinnedToCore(vRadarTask,           // Your original task
                            "Radar Service",
                            4096,
                            NULL,
                            10,
                            NULL,
                            1);

    xTaskCreatePinnedToCore(vDisplayUpdateTask,   // Separate display task
                            "Display Update",
                            2048,
                            NULL,
                            5,
                            NULL,
                            0);
    */
}

/**
 * Integration steps:
 *
 * 1. Add to CMakeLists.txt (main component):
 *    In the idf_component_register() call, add ui_radar_display.c to SRCS
 *
 * 2. In main.c, replace:
 *      ui_skoona_page(screen);
 *    with:
 *      radar_display_create_ui(screen);
 *
 * 3. In mmwave.c, add display update after getting targets:
 *      target_count = radar_sensor_get_targets(&radar, targets);
 *
 *      bsp_display_lock(0);
 *      radar_display_update(targets, target_count);
 *      bsp_display_unlock();
 *
 * 4. Add includes to mmwave.c:
 *      #include "bsp/esp-bsp.h"
 *      #include "ui_radar_display.h"
 */
