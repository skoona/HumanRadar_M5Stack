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
#include "freertos/semphr.h"
#include "iot_button.h"
#include "lvgl.h"
#include "nvs_flash.h"
#include "protocol_examples_common.h"
#include"esp_lv_decoder.h"
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

/* List storage contents to console 
*/
esp_err_t fileList() {
	/* Get file name in storage */
	struct dirent *p_dirent = NULL;
	struct stat st;

	DIR *p_dir_stream = opendir("/spiffs");
    if (p_dir_stream == NULL) {
		ESP_LOGE(TAG, "Failed to open mount: %s", "/spiffs");
		return ESP_FAIL;
	}

    char files[256] = {"/spiffs/"};

	/* Scan files in storage */
	while (true) {
		p_dirent = readdir(p_dir_stream);
		if (NULL != p_dirent) {
			strcpy(files, "/spiffs/");
            strcat(files,p_dirent->d_name);
            if (stat(files, &st) == 0) {
				ESP_LOGI(TAG, "Filename: [%d] %s", st.st_size,
						 p_dirent->d_name);
			}
			else {
				ESP_LOGI(TAG, "Filename: %s", p_dirent->d_name);
			}
		} else {
			closedir(p_dir_stream);
			break;
		}
	}
	return ESP_OK;
}

/* Called on button press */
void btn_handler(void *button_handle, void *usr_data) {
		int button_index = (int)usr_data;

		switch (button_index) {
		case 0:
			fileList();
			break;
		case 1:
			logMemoryStats("Button 1 pressed");
			break;
		case 2:
			logMemoryStats("Button 2 pressed");
			break;
		}
		ESP_LOGI(TAG, "Button %d pressed", button_index);
}


// serivce lvgl events time requirements
uint32_t milliseconds() {
    int64_t v64 = esp_timer_get_time();
    return (v64 / 1000);
}

void app_main(void)
{
	lv_obj_t *screen = NULL;

	vTaskDelay(pdMS_TO_TICKS(4000));
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
	lv_display_t *disp = bsp_display_start_with_config(&cfg);

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

	bsp_display_lock(0);
		screen = lv_disp_get_scr_act(disp);
		ui_skoona_page(screen);
	bsp_display_unlock();

	start_mmwave(NULL);
	logMemoryStats("App Main startup complete");
}
