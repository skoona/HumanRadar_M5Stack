/*
 * skoona@gmail.com
 * 12/29/2025
 * HumanRadar Template
 * file: main.c
 */

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "humanRadarRD_03D.h"
#include"math.h"


bool radar_targets_moved(radar_target_t *currentTargets, radar_target_t *priorTargets, int targetId) {
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

	// Initialize the radar sensor
    esp_err_t ret = radar_sensor_init(&radar, CONFIG_UART_PORT, CONFIG_UART_RX_GPIO, CONFIG_UART_TX_GPIO);
    if (ret != ESP_OK)
    {
        switch (ret)
        {
        case ESP_OK:
            ESP_LOGI("RD-03D", "Initialization successful");
            break;
        case ESP_ERR_INVALID_ARG:
            ESP_LOGE("RD-03D", "Invalid arguments provided");
            break;
        default:
            ESP_LOGE("RD-03D", "Initialization failed: %s", esp_err_to_name(ret));
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
    while (1)
    {
        if (radar_sensor_update(&radar))
        {
			memcpy(priorTargets, targets, sizeof(targets));
			target_count = radar_sensor_get_targets(&radar, targets);
            for (int idx = 0; idx < target_count; idx++)
            {
				if (radar_targets_moved(targets, priorTargets, idx)) {
					ESP_LOGI("RD-03D", "[%d]Target detected at (%.1f, %.1f) mm, distance: %.1f mm", idx, targets[idx].x, targets[idx].y, targets[idx].distance);
                    ESP_LOGI("RD-03D", "[%d]Position: %s", idx, targets[idx].position_description);
                    ESP_LOGI("RD-03D", "[%d]Angle: %.1f degrees, Distance: %.1f mm, Speed: %.1f mm/s", idx, targets[idx].angle, targets[idx].distance, targets[idx].speed);
				}
			}
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void start_mmwave(void *pvParameters)
{
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_log_level_set("*", ESP_LOG_INFO);
    xTaskCreatePinnedToCore(vRadarTask, "Radar Service", 4096, pvParameters, 10, NULL, 1);
}
