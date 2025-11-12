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

extern void example_lvgl_demo_ui(lv_obj_t *scr);

// Function to start the HTTP server
extern httpd_handle_t start_http_listener(void); 

// Example Wi-Fi initialization (replace with your actual Wi-Fi setup)
extern void wifi_init_sta(void);
extern void unifi_async_api_request(esp_http_client_method_t method, char * path);
extern void unifi_api_request_le2k(esp_http_client_method_t method, char * path);
extern void unifi_api_request_gt2k(esp_http_client_method_t method, char * path);
esp_err_t display_received_image(char *path);
esp_err_t fileList();
esp_err_t writeBinaryImageFile(char *path, void *buffer, int bufLen);


	static const char *TAG = "sknSensors";
static lv_obj_t *screen = NULL;
int global_image_scale = 80;

	/* Decode base64 buffer and save and to file.
	 * Assumed to come from: JSON 'alarm.thumbnail'
	 */
esp_err_t writeBase64Buffer(char *path, const unsigned char *input_buffer) {

	size_t input_len = strlen((const char *)input_buffer)-23;
	unsigned char *output_buffer;
	size_t output_len;
    const unsigned char *pInputBuffer = &input_buffer[23];

    output_len = ((input_len + 3) / 3) * 4 + 1;

	// Allocate memory for the output buffer
	output_buffer = (unsigned char *)calloc(output_len, sizeof(unsigned char));
	if (output_buffer == NULL) {
		ESP_LOGE(TAG, "Failed to allocate [%d:%d] bytes for base64 output buffer for: %s",input_len,output_len, path);
		return ESP_FAIL;
	}

	// 'data:image/jpeg;base64,' = 23 bytes
	int ret = mbedtls_base64_decode(output_buffer, output_len, &output_len,	pInputBuffer, input_len);

	if (ret == 0) {
		// Decoding successful.
		ret = writeBinaryImageFile(path, output_buffer, output_len);
	} else {
		ESP_LOGE(TAG, "Failed to decode base64 contents for: %s", path);

		ret = ESP_FAIL;
	}
	free(output_buffer); // Clean up memory

	return ret;
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
	display_received_image(path); 
    return ESP_OK;
}

/* Display image file from spiffs
*/
esp_err_t display_received_image(char *path) {
	char image[256] = {"S:"};
	lv_obj_t *img = lv_img_create(screen);
    if(img == NULL) {
		ESP_LOGE(TAG, "Failed to create image for %s", path);
		return ESP_FAIL;
    }
	sprintf(image, "S:%s", path);
	lv_img_set_src(img, image);
	lv_image_set_scale(img, global_image_scale); // 80
	lv_obj_center(img);
    return ESP_OK;
}

/* Called on button press */
static void btn_handler(void *button_handle, void *usr_data)
{
    static bool oneShot = false;
    int button_index = (int)usr_data;
	switch (button_index) {
	case 0:
		// Get All Cameras
		unifi_async_api_request(HTTP_METHOD_GET, "https://10.100.1.1/proxy/protect/integration/v1/cameras");
		break;
	case 1:
        if (oneShot) {
		    // Get Garage Snapshot -- Binary
            global_image_scale = 192;
		    unifi_api_request_gt2k(HTTP_METHOD_GET, "https://10.100.1.1/proxy/protect/integration/v1/cameras/65b2e8d400858f03e4014f3a/snapshot");
        }
        oneShot = true;
		break;
	case 2:
	    // Get Front Door Snapshot -- BINARY
        global_image_scale = 80;
		unifi_api_request_gt2k(HTTP_METHOD_GET, "https://10.100.1.1/proxy/protect/integration/v1/cameras/6096c66202197e0387001879/snapshot");
		break;
}

    ESP_LOGI(TAG, "Button %d pressed", button_index);
}

/* Gather hash from JSON config.json
   - lookup device and generate URL
   - submit URL to cause image display
*/
esp_err_t handleAlarms(char *alert_type, char *device) {

    return ESP_OK;
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


    ESP_LOGI("example", "Display LVGL animation");
    bsp_display_lock(0);
	screen = lv_disp_get_scr_act(NULL);

	example_lvgl_demo_ui(screen);

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
}
