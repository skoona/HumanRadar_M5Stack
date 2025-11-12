/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

/**
 * @file
 * @brief BSP Display Example
 * @details Show an image on the screen with a simple startup animation (LVGL)
 * @example https://espressif.github.io/esp-launchpad/?flashConfigURL=https://espressif.github.io/esp-bsp/config.toml&app=display-
 */

#include "bsp/esp-bsp.h"
#include "esp_event.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_lv_decoder.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"
#include "lvgl.h"
#include "mbedtls/base64.h"
#include "nvs_flash.h"
#include "protocol_examples_common.h"
#include <dirent.h>
#include <esp_heap_caps.h>
#include <esp_http_server.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static QueueHandle_t imageQueue;
static QueueHandle_t urlQueue;

extern void	ui_skoona_page(lv_obj_t *scr);

// Function to start the HTTP server
extern httpd_handle_t start_http_listener(void); 

// Example Wi-Fi initialization (replace with your actual Wi-Fi setup)
extern void wifi_init_sta(void);
extern void unifi_async_api_request(esp_http_client_method_t method, char * path);
extern void unifi_api_request_le2k(esp_http_client_method_t method, char * path);
extern void unifi_api_request_gt2k(esp_http_client_method_t method, char * path);
esp_err_t fileList();
esp_err_t writeBinaryImageFile(char *path, void *buffer, int bufLen);


static const char *TAG = "sknSensors";
static lv_obj_t *screen = NULL;
int global_image_scale = 80;

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

/* Write  Binary file and display as current image
 * example: *path = "S:/spiffs/listener_event.jpg";
*/
esp_err_t writeBinaryImageFile(char *path, void *buffer, int bufLen) {
	uint written = 0;
	FILE *listener_event_file = NULL;
    
    listener_event_file = fopen(path, "wb");
    if (listener_event_file == NULL) {
        ESP_LOGE(TAG, "Failed to open %s file for writing", path);
		return ESP_FAIL;
    } else {
	    // written = fwrite(buffer, sizeof(char),bufLen, listener_event_file);
		written = fwrite(buffer, bufLen, 1, listener_event_file);
		fclose(listener_event_file);
	    ESP_LOGI(TAG, "File written, name: %s, bytes: %d", path, written);
	}
	// Create image
	xQueueSend(imageQueue, path, 0);
	return ESP_OK;
}

/* Called on button press */
static void btn_handler(void *button_handle, void *usr_data)
{
    static bool oneShot = false;
    int button_index = (int)usr_data;
    char url[254];
	switch (button_index) {
	case 0:
		// Get All Cameras
		unifi_async_api_request(HTTP_METHOD_GET, "https://10.100.1.1/proxy/protect/integration/v1/cameras");
		break;
	case 1:
        if (oneShot) {
		    // Get Garage Snapshot -- Binary
            global_image_scale = 192;
            strcpy(url,"https://10.100.1.1/proxy/protect/integration/v1/cameras/65b2e8d400858f03e4014f3a/snapshot");
	        xQueueSend(urlQueue, url, 10);
        }
        oneShot = true;
		break;
	case 2:
	    // Get Front Door Snapshot -- BINARY
        global_image_scale = 80;
		strcpy(url, "https://10.100.1.1/proxy/protect/integration/v1/cameras/6096c66202197e0387001879/snapshot");
		xQueueSend(urlQueue, url, 10);
		break;
}

    ESP_LOGI(TAG, "Button %d pressed", button_index);
}

/* Gather hash from JSON config.json
   - lookup device and generate URL
   - submit URL to cause image display
   - get constants from config.json in future
*/
esp_err_t handleAlarms(char *device) {
    char *alarm_id = NULL;

	if (strcmp("E063DA00602B", device) == 0) { // front door
		alarm_id = "6096c66202197e0387001879";
	} else if (strcmp("70A7413F0FD7", device) == 0) { // Garage
		alarm_id = "65b2e8d400858f03e4014f3a";
	} else if (strcmp("F4E2C60C4E6A", device) == 0) { // Kitchen and Dining
		alarm_id = "68cf46180060ac03e42f9125";
	} else if (strcmp("70A7410B7643", device) == 0) { // South View
		alarm_id = "6355cf55027c9603870029ed";
	} else if (strcmp("70A7410B7593", device) == 0) { // East View
		alarm_id = "63559ad5007a960387002880";
	} else {
        ESP_LOGE(TAG, "Unknown Device: %s", device);
        return ESP_FAIL;
    }

	global_image_scale = 80;

    // lv_obj_t *spinner = lv_spinner_create(lv_scr_act());
	// lv_spinner_set_anim_params(spinner,1000, 60);
    // lv_obj_set_size(spinner, 100, 100);
	// lv_obj_center(spinner);

	char url[254];

    sprintf(url,"https://10.100.1.1/proxy/protect/integration/v1/cameras/%s/snapshot", alarm_id);
	xQueueSend(urlQueue, url, 10);
	return ESP_OK;
}

static void vURLTask(void *pvParameters) {
	char url[288]; // Used to receive data
	BaseType_t xReturn; // Used to receive return value
    QueueHandle_t urlQueue = pvParameters;

	while (1) {
		xReturn = xQueueReceive(urlQueue, url, pdMS_TO_TICKS(3000));
		if (xReturn == pdTRUE) {
			ESP_LOGI("URLTask", "Received URL: %s", url);
			unifi_api_request_gt2k(HTTP_METHOD_GET, url);
		}
		vTaskDelay(10);
	}
}

static void vImageTask(void *pvParameters) {
	char path[256];		// Used to receive data
	char image[288] = {"S:"};
	BaseType_t xReturn; // Used to receive return value
	QueueHandle_t ImageQueue = pvParameters;
	static lv_style_t style_max_height;
	static lv_style_t style_max_width;

	while (1) {
		xReturn = xQueueReceive(ImageQueue, path, pdMS_TO_TICKS(3000));
		if (xReturn == pdTRUE) {
            ESP_LOGI("ImageTask", "Received image file: %s", path);
			lv_obj_t *img = lv_img_create(lv_screen_active());

			if (img != NULL) {
                lv_obj_set_style_bg_color(lv_screen_active(), lv_color_white(), LV_PART_MAIN);
				sprintf(image, "S:%s", path); 
				lv_img_set_src(img, image); // 240 * 320                

                lv_style_init(&style_max_height);
                lv_style_set_y(&style_max_height, 315);
				lv_obj_set_height(img, lv_pct(100));
				lv_obj_add_style(img, &style_max_height, LV_STATE_DEFAULT);

				lv_style_init(&style_max_width);
                lv_style_set_y(&style_max_width, 235);
                lv_obj_set_width(img, lv_pct(100));
                lv_obj_add_style(img, &style_max_height, LV_STATE_DEFAULT);

				lv_image_set_inner_align(img, LV_IMAGE_ALIGN_STRETCH);
				// lv_image_set_scale(img, global_image_scale); // 80
				lv_obj_center(img);
			} else {
				ESP_LOGE(TAG, "Failed to create image for %s", path);
			}
		} 
		vTaskDelay(10);
	}
}

void app_main(void)
{

	vTaskDelay(pdMS_TO_TICKS(4000));

    ESP_LOGI(TAG, "[APP] Startup..");
    ESP_LOGI(TAG, "[APP] Free memory: %" PRIu32 " bytes", esp_get_free_heap_size());

    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("transport", ESP_LOG_VERBOSE);

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(example_connect());

	imageQueue = xQueueCreate(16, 256);
	urlQueue = xQueueCreate(16, 256);
	if (imageQueue != NULL) {
		xTaskCreatePinnedToCore(vImageTask, "ImageTask", 4096, imageQueue, 2, NULL, 0);
		xTaskCreatePinnedToCore(vURLTask, "vURLTask", 4096, urlQueue, 2, NULL, 0);
	} else {
		ESP_LOGE(TAG, "Display Queues Failed.");
	}
	// Start the HTTP server
    start_http_listener();

    // bsp_display_start();

    /* Initialize display and LVGL */
    bsp_display_cfg_t cfg = {
        .lvgl_port_cfg = ESP_LVGL_PORT_INIT_CONFIG(),
        .buffer_size = BSP_LCD_H_RES * CONFIG_BSP_LCD_DRAW_BUF_HEIGHT,
        .double_buffer = 0,
        .flags = {
            .buff_dma = true,
        }
    };
    bsp_display_start_with_config(&cfg);

    /* Set display brightness to 100% */
    bsp_display_backlight_on();

    /* Mount SPIFFS */
    bsp_spiffs_mount();

    // lv_split_jpeg_init();
    esp_lv_decoder_handle_t decoder_handle = NULL;
    esp_lv_decoder_init(&decoder_handle); //Initialize this after lvgl starts

    bsp_display_lock(0);
	screen = lv_disp_get_scr_act(NULL);

	ui_skoona_page(screen);

	bsp_display_unlock();
    bsp_display_backlight_on();
   
	/* Initialize all available buttons */
	button_handle_t btns[BSP_BUTTON_NUM] = {NULL};
	bsp_iot_button_create(btns, NULL, BSP_BUTTON_NUM);
	
	/* Register a callback for button press */
	for (int i = 0; i < BSP_BUTTON_NUM; i++) {
	    iot_button_register_cb(btns[i], BUTTON_PRESS_DOWN, btn_handler, (void *) i);
	}

    // show spiffs contents
    fileList();

	vTaskDelay(pdMS_TO_TICKS(10000));
	// ESP_LOGI(TAG, "Service LVGL loop");
	// while (1) {
	// 	/* Provide updates to currently-displayed Widgets here. */
	// 	lv_timer_handler();
	// 	vTaskDelay(pdMS_TO_TICKS(5));
	// }
}
