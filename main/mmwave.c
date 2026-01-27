/*
 * skoona@gmail.com
 * 12/29/2025
 * HumanRadar Template
 * file: main.c
 */

#include "bsp/esp-bsp.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"
#include "freertos/task.h"
#include "humanRadarRD_03D.h"
#include "math.h"
#include "ui_radar_display.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

extern bool logoDone;

bool hasTargetMoved(radar_target_t *currentTargets, radar_target_t *priorTargets, int targetId) {
	if (currentTargets[targetId].detected && priorTargets[targetId].detected) {
		float deltaX = currentTargets[targetId].x - priorTargets[targetId].x;
		float deltaY = currentTargets[targetId].y - priorTargets[targetId].y;
		float distanceMoved = sqrtf(deltaX * deltaX + deltaY * deltaY);
        if (distanceMoved >= 75.0f)  // Movement threshold in mm
        {
            return true; // A target has moved beyond the threshold
        }
	} else if (currentTargets[targetId].detected != priorTargets[targetId].detected) {
		return true; // Target appearance/disappearance is considered movement
	}
	return false; // No significant movement detected
}

void vRadarTask(void *pvParameters) {
    radar_sensor_t radar;
    radar_target_t targets[RADAR_MAX_TARGETS];
	radar_target_t priorTargets[RADAR_MAX_TARGETS];
	
    while (!logoDone) {
		vTaskDelay(pdMS_TO_TICKS(1000)); // wait for logo screen to finish
	}

	bsp_display_lock(0);       
        lv_obj_set_style_bg_color((lv_obj_t *)pvParameters, lv_color_black(), LV_PART_MAIN);
        lv_obj_set_style_bg_opa((lv_obj_t *)pvParameters, LV_OPA_COVER, LV_PART_MAIN);
		radar_display_create_ui((lv_obj_t *)pvParameters); // Add this
		// Example: Slide left, 1000ms duration, 0ms delay
		lv_screen_load_anim((lv_obj_t *)pvParameters, LV_SCR_LOAD_ANIM_MOVE_LEFT, 3000, 0, true);
	bsp_display_unlock();

    // Initialize the radar sensor
    esp_err_t ret = radar_sensor_init(&radar, CONFIG_UART_PORT, CONFIG_UART_RX_GPIO, CONFIG_UART_TX_GPIO);
    if (ret != ESP_OK) {
        switch (ret) {
        case ESP_OK:
            ESP_LOGI("RD-03D", "Initialization successful");
            break;
        case ESP_ERR_INVALID_ARG:
            ESP_LOGE("RD-03D", "Invalid arguments provided");
            break;
        default:
            ESP_LOGE("RD-03D", "Initialization failed: %s",
                        esp_err_to_name(ret));
            break;
        }
        vTaskDelete(NULL);
    }

    // Start UART communication
    ret = radar_sensor_begin(&radar, CONFIG_UART_SPEED_BPS);
    if (ret != ESP_OK)
    {
        ESP_LOGE("RD-03D", "Failed to start radar sensor");
        vTaskDelete(NULL);
    }

    // tarck upto three targets -- return error if already in multi mode
    radar_sensor_set_multi_target_mode(&radar, true);

	// Configure for security application (longer retention)
	radar_sensor_set_retention_times(&radar, 10000, 500); // 10s detection, 0.5s absence

	ESP_LOGI("RD-03D", "Sensor is active, starting main loop.");
    // Main loop
    int target_count = 0;
    bool hasMoved = false;
    while (1)
    {
        if (radar_sensor_update(&radar))
        {
			memcpy(priorTargets, targets, sizeof(targets));
			target_count = radar_sensor_get_targets(&radar, targets);
            for (int idx = 0; idx < target_count; idx++)
            {
                hasMoved = hasTargetMoved(targets, priorTargets, idx);
				if (hasMoved) {
                    ESP_LOGI("RD-03D", "[%d] X:%.0f Y:%.0f D:%.0f A:%.1f S:%.0f",
                            idx, targets[idx].x, targets[idx].y,
                            targets[idx].distance, targets[idx].angle,
                            targets[idx].speed);					
				}
                // Add display update
                bsp_display_lock(0);
                radar_display_update(targets, idx, hasMoved);
                bsp_display_unlock();
			}
        }
        vTaskDelay(pdMS_TO_TICKS(250));
    }
}

void start_mmwave(void *pvParameters)
{
	xTaskCreatePinnedToCore(vRadarTask, "Radar Service", 4096, pvParameters, 10, NULL, 1);
}
