/**
 * @file main.c
 * @brief BSP Display Example
 * @details Monitor the Rd-03D mmWave Sensor.
 */

#include "bsp/esp-bsp.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_lv_decoder.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"
#include "iot_button.h"
#include "lvgl.h"
#include "nvs_flash.h"
#include "protocol_examples_common.h"
#include "ui_radar_integration.h"
#include <dirent.h>
#include <esp_heap_caps.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

extern void	ui_skoona_page(lv_obj_t *scr);
extern void start_mmwave(void *pvParameters);
static const char *TAG = "skoona.net";
bool logoDone = false; // control when radar screen is created
static lv_display_t *g_disp = NULL;

void logMemoryStats(char *message) {
	char buffer[1024] = {0};

	vTaskList(buffer);

	ESP_LOGI(TAG, "[APP] %s...", message);
	ESP_LOGI(TAG, "[APP] Free memory: %" PRIu32 " bytes",
			 esp_get_free_heap_size());
	ESP_LOGI(TAG, "Internal free heap size: %ld bytes",
			 esp_get_free_internal_heap_size());
	ESP_LOGI(TAG, "PSRAM    free heap size: %ld bytes",
			 esp_get_free_heap_size() - esp_get_free_internal_heap_size());
	ESP_LOGI(TAG, "Total    free heap size: %ld bytes",
			 esp_get_free_heap_size());
	ESP_LOGI(TAG,
			 "Task List:\nTask Name\tStatus\tPrio\tHWM\tTask\tAffinity\n%s",
			 buffer);
}

/**
 * EXAMPLE: Modified button handler that switches displays
 */
void btn_handler(void *button_handle, void *usr_data) {
	radar_btn_handler_example(button_handle, usr_data, g_disp);
}

// serivce lvgl events time requirements
uint32_t milliseconds() {
    int64_t v64 = esp_timer_get_time();
    return (v64 / 1000);
}

void app_main(void)
{
	logMemoryStats("App Main started");

    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("transport", ESP_LOG_VERBOSE);

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

	/* Initialize display and LVGL */	
	bsp_display_cfg_t cfg = {
		.lvgl_port_cfg = ESP_LVGL_PORT_INIT_CONFIG(),
		.buffer_size = BSP_LCD_H_RES * BSP_LCD_V_RES / 10,
		.double_buffer = false,
		.flags = {
		    .buff_dma = true,
			.buff_spiram = false,
	    },
	};
	g_disp = bsp_display_start_with_config(&cfg);

	ESP_ERROR_CHECK(example_connect());
	
    /* Set display brightness to 100% */
    bsp_display_backlight_on();
	bsp_display_brightness_set(100);
    
    esp_lv_decoder_handle_t decoder_handle = NULL;
    esp_lv_decoder_init(&decoder_handle); //Initialize this after lvgl starts
    lv_tick_set_cb(milliseconds);
	
	/* Mount SPIFFS */
	bsp_spiffs_mount();
		
	/* Initialize all available buttons */
#define BUTTON_NUM 3
	button_handle_t btns[BUTTON_NUM] = {NULL};
	bsp_iot_button_create(btns, NULL, BUTTON_NUM);

	/* Register a callback for button press */
	for (int i = 0; i < BUTTON_NUM; i++) {
		iot_button_register_cb(btns[i], BUTTON_PRESS_DOWN, NULL, btn_handler, (void *) i);
	}

	// Show splash screen with Skoona logo animation
	bsp_display_lock(0);
	lv_obj_t *screen = lv_disp_get_scr_act(g_disp);
	ui_skoona_page(screen);
	bsp_display_unlock();

	ESP_LOGI(TAG, "Splash screen displayed, waiting for animation to complete...");

	// Wait for logo animation to complete
	while (!logoDone) {
		vTaskDelay(pdMS_TO_TICKS(100));
	}

	ESP_LOGI(TAG, "Animation complete, waiting 4 seconds...");
	vTaskDelay(pdMS_TO_TICKS(4000));

	// Clean up splash screen and initialize radar display
	bsp_display_lock(0);
	lv_obj_clean(screen);  // Remove all objects from screen
	bsp_display_unlock();

	ESP_LOGI(TAG, "Initializing radar display...");
	// Initialize radar display system (starts in SWEEP mode)
	radar_display_init(g_disp, DISPLAY_MODE_SWEEP);

	start_mmwave(NULL);
	logMemoryStats("App Main startup complete");
}
