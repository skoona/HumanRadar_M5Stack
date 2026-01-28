/*
 * ui_radar_integration.c
 * Integration example showing how to use both radar displays
 * and switch between them
 */

#include "bsp/esp-bsp.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "humanRadarRD_03D.h"
#include "ui_radar_display.h"
#include "ui_radar_sweep.h"

extern void logMemoryStats(char *message);

static const char *TAG = "RadarIntegration";

// Display mode
typedef enum {
    DISPLAY_MODE_LIST,      // List view (ui_radar_display)
    DISPLAY_MODE_SWEEP,     // Radar sweep view (ui_radar_sweep)
} display_mode_t;

static display_mode_t current_mode = DISPLAY_MODE_SWEEP;
static lv_obj_t *current_screen = NULL;

/**
 * @brief Switch between display modes
 *
 * This function can be called from a button handler to toggle views
 */
void radar_switch_display_mode(lv_display_t *disp)
{
    bsp_display_lock(0);

    // Get the active screen
	lv_obj_t *screen = lv_disp_get_scr_act(disp);

	// Clean up current display
    if (current_mode == DISPLAY_MODE_LIST) {
        radar_display_delete_ui();
        current_mode = DISPLAY_MODE_SWEEP;
        ESP_LOGI(TAG, "Switching to SWEEP mode");
    } else {
        radar_sweep_delete_ui();
        current_mode = DISPLAY_MODE_LIST;
        ESP_LOGI(TAG, "Switching to LIST mode");
    }

    // Clear screen
    lv_obj_clean(screen);

    // Create new display
    if (current_mode == DISPLAY_MODE_LIST) {
        radar_display_create_ui(screen);
    } else {
        radar_sweep_create_ui(screen);
    }

    bsp_display_unlock();
}

/**
 * @brief Update the current display with radar data
 *
 * Call this from your radar task after receiving new data
 */
void radar_update_current_display(radar_target_t *targets, int targetId, bool hasMoved)
{
    if (targets == NULL) {
        return;
    }

    bsp_display_lock(0);

    if (current_mode == DISPLAY_MODE_LIST) {
        radar_display_update(targets, targetId, hasMoved);
    } else {
        radar_sweep_update(targets, targetId, hasMoved);
    }

    bsp_display_unlock();
}

/**
 * @brief Initialize radar display system
 *
 * Call this once during startup, after LVGL is initialized
 */
void radar_display_init(lv_display_t *disp, display_mode_t initial_mode)
{
    current_mode = initial_mode;

    bsp_display_lock(0);

    lv_obj_t *screen = lv_disp_get_scr_act(disp);
    current_screen = screen;

	if (current_mode == DISPLAY_MODE_LIST) {
        radar_display_create_ui(screen);
        ESP_LOGI(TAG, "Initialized in LIST mode");
    } else {
        radar_sweep_create_ui(screen);
        ESP_LOGI(TAG, "Initialized in SWEEP mode");
    }

    bsp_display_unlock();
}

/**
 * @brief Get current display mode
 */
display_mode_t radar_get_display_mode(void)
{
    return current_mode;
}

/**
 * EXAMPLE: Modified button handler that switches displays
 *
 * Add this to your btn_handler in main.c:
 */
void radar_btn_handler_example(void *button_handle, void *usr_data, lv_display_t *disp)
{
    int button_index = (int)usr_data;

    switch (button_index) {
    case 0:
        // Button 0: Switch display mode
        radar_switch_display_mode(disp);
        break;
    case 1:
        // Button 1: Toggle sweep animation (only in sweep mode)
        if (current_mode == DISPLAY_MODE_SWEEP) {
            // You could add a static variable to track state
            static bool animation_running = true;
            if (animation_running) {
                radar_sweep_stop_animation();
                ESP_LOGI(TAG, "Sweep animation stopped");
            } else {
                radar_sweep_start_animation();
                ESP_LOGI(TAG, "Sweep animation started");
            }
            animation_running = !animation_running;
        }
        break;
    case 2:
        // Button 2: Your custom action
        logMemoryStats("Button 2 pressed");
        break;
    }
}

/**
 * EXAMPLE: Complete integration in main.c
 *
 * Replace your current display initialization with:
 */
/*
void app_main_integrated(void)
{
    // ... all your existing initialization ...

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

    // Initialize radar display system (starts in SWEEP mode)
    radar_display_init(disp, DISPLAY_MODE_SWEEP);

    // ... button setup ...

    // Start radar task
    start_mmwave(NULL);
}
*/

/**
 * EXAMPLE: Modified radar task that updates the display
 *
 * In your mmwave.c, modify vRadarTask:
 */
/*
void vRadarTask(void *pvParameters)
{
    radar_sensor_t radar;
    radar_target_t targets[RADAR_MAX_TARGETS];
    radar_target_t priorTargets[RADAR_MAX_TARGETS];

    // ... initialization ...

    int target_count = 0;
    while (1) {
        if (radar_sensor_update(&radar)) {
            memcpy(priorTargets, targets, sizeof(targets));
            target_count = radar_sensor_get_targets(&radar, targets);

            // Update each target that has moved
            for (int idx = 0; idx < RADAR_MAX_TARGETS; idx++) {
                bool hasMoved = false;

                if (targets[idx].detected) {
                    // Check if target has moved significantly
                    if (!priorTargets[idx].detected ||
                        fabsf(targets[idx].x - priorTargets[idx].x) > 50 ||
                        fabsf(targets[idx].y - priorTargets[idx].y) > 50) {
                        hasMoved = true;
                    }

                    // Update display for this target
                    radar_update_current_display(targets, idx, hasMoved);
                }
            }

            // Update target count (for sweep mode)
            if (radar_get_display_mode() == DISPLAY_MODE_SWEEP) {
                bsp_display_lock(0);
                radar_sweep_update_info(target_count);
                bsp_display_unlock();
            }
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
*/
