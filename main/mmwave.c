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
#include "ui_radar_integration.h"
#include "ui_radar_sweep.h"
#include <math.h>
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
	radar_target_t priorTargets[RADAR_MAX_TARGETS] = {0};

    // wait for logo sceen to finish
	while (!logoDone) {
		vTaskDelay(pdMS_TO_TICKS(1000));
	}

	// Initialize radar display (starts in SWEEP mode)
	radar_display_init((lv_display_t *)pvParameters, DISPLAY_MODE_SWEEP);

	// Initialize radar sensor
	esp_err_t ret = radar_sensor_init(&radar, CONFIG_UART_PORT,
									  CONFIG_UART_RX_GPIO, CONFIG_UART_TX_GPIO);
	if (ret != ESP_OK) {
		ESP_LOGE("mmWave", "Radar initialization failed: %s", esp_err_to_name(ret));
		vTaskDelete(NULL);
	}

	ret = radar_sensor_begin(&radar, CONFIG_UART_SPEED_BPS);
	if (ret != ESP_OK) {
		ESP_LOGE("mmWave", "Failed to start radar sensor");
		vTaskDelete(NULL);
	}

	// Configure radar
	radar_sensor_set_multi_target_mode(&radar, true);
	radar_sensor_set_retention_times(&radar, 10000, 500);

	ESP_LOGI("mmWave", "Sensor is active, starting main loop.");

	int target_count = 0;

	while (1) {
		if (radar_sensor_update(&radar)) {
			// Save previous state
			memcpy(priorTargets, targets, sizeof(targets));

			// Get current targets
			target_count = radar_sensor_get_targets(&radar, targets);

			// Update each target
			for (int idx = 0; idx < RADAR_MAX_TARGETS; idx++) {
				bool hasMoved = false;

				if (targets[idx].detected) {
					// Check if target has moved significantly (>50mm in any
					// direction)
					if (!priorTargets[idx].detected ||
						fabsf(targets[idx].x - priorTargets[idx].x) > 50.0f ||
						fabsf(targets[idx].y - priorTargets[idx].y) > 50.0f) {
						hasMoved = true;
					}

					// Update display
					radar_update_current_display(targets, idx, hasMoved);

					// Log movement
					if (hasMoved) {
						ESP_LOGI("mmWave",
								 "[%d] X:%.0f Y:%.0f D:%.0f A:%.1f S:%.0f %s",
								 idx, targets[idx].x, targets[idx].y,
								 targets[idx].distance, targets[idx].angle,
								 targets[idx].speed,
								 targets[idx].position_description);
					}
				} else if (priorTargets[idx].detected) {
					// Target lost
					radar_update_current_display(targets, idx, false);
					ESP_LOGI("mmWave", "[%d] Target lost", idx);
				}
			}

			// Update target count info (for sweep display)
			if (radar_get_display_mode() == DISPLAY_MODE_SWEEP) {
				bsp_display_lock(0);
				radar_sweep_update_info(target_count);
				bsp_display_unlock();
			}
		}
		vTaskDelay(pdMS_TO_TICKS(500));
	}
}

void start_mmwave(void *pvParameters)
{
	xTaskCreatePinnedToCore(vRadarTask, "Radar Service", 4096, pvParameters, 10, NULL, 1);
}
