/**
 * @file main.c
 * @brief BSP Display Example
 * @details Show an related image on the screen after a, Protect API sends an Alarm webhook.
 */

#include "bsp/esp-bsp.h"
#include "esp_event.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_lv_decoder.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"
#include "freertos/semphr.h"
#include "iot_button.h"
// #include "jpeg_decoder.h"
#include "lvgl.h"
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

QueueHandle_t imageQueue;
QueueHandle_t urlQueue;
SemaphoreHandle_t xMutex;

extern void	ui_skoona_page(lv_obj_t *scr);
extern httpd_handle_t start_http_listener(void); 
extern void wifi_init_sta(void);
extern void unifi_async_api_request(esp_http_client_method_t method, char * path);
extern void unifi_api_request_gt2k(esp_http_client_method_t method, char * path);
static const char *TAG = "skoona.net";

void standBy(char *message) {
	static lv_style_t style_red;
	lv_obj_t *standby;

	if (message == NULL) return;
	
	bsp_display_lock(0);
		
	standby = lv_label_create(lv_scr_act());
	lv_label_set_text(standby, message);
	lv_obj_set_style_text_font(standby, &lv_font_montserrat_32, 0);
	lv_style_init(&style_red);
	lv_style_set_bg_color(&style_red, lv_color_make(128, 128, 128));
	lv_style_set_bg_opa(&style_red, LV_OPA_COVER);
	lv_style_set_text_color(&style_red, lv_color_make(0xff, 0x00, 0x00)); // Red Color
	lv_obj_add_style(standby, &style_red, LV_PART_MAIN);
	lv_obj_center(standby);

	bsp_display_unlock();
	
	// play_wave_file(SAMPLE_FILE);
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
	int event_file = 0;
	if (xSemaphoreTake(xMutex, portMAX_DELAY) == pdTRUE) {
		event_file = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
		if (event_file == -1) {
			ESP_LOGE(TAG, "Failed to open %s file for writing", path);
			xSemaphoreGive(xMutex);
			return ESP_FAIL;
		} else {
			// written = fwrite(buffer, sizeof(char),bufLen,
			// listener_event_file);
			// written = fwrite(buffer, bufLen, 1, event_file);
			written = write(event_file, buffer, bufLen);
			close(event_file);
			ESP_LOGI(TAG, "File written, name: %s, bytes: %d", path, written);
		}
		xSemaphoreGive(xMutex);
		// Create image
		xQueueSend(imageQueue, path, 0);
	}
	return ESP_OK;
}

/* Called on button press */
static void btn_handler(void *button_handle, void *usr_data) {
		static bool oneShot = false;
		int button_index = (int)usr_data;
		char url[254];

		switch (button_index) {
		case 0:
			// Get All Cameras
			unifi_async_api_request(HTTP_METHOD_GET,
									CONFIG_PROTECT_API_ENDPOINT);
			fileList();

			break;
		case 1:
			if (oneShot) {
				// Get Garage Snapshot -- Binary
				sprintf(url, "%s/65b2e8d400858f03e4014f3a/snapshot",
						CONFIG_PROTECT_API_ENDPOINT);
				xQueueSend(urlQueue, url, 10);
			}			
			break;
		case 2:
			// Get Front Door Snapshot -- BINARY
			sprintf(url, "%s/6096c66202197e0387001879/snapshot",
					CONFIG_PROTECT_API_ENDPOINT);
			xQueueSend(urlQueue, url, 10);
			break;
		}
		oneShot = true;
		ESP_LOGI(TAG, "Button %d pressed", button_index);

}

static void vURLTask(void *pvParameters) {
	char url[288]; // Used to receive data
	BaseType_t xReturn; // Used to receive return value
    QueueHandle_t urlQueue = pvParameters;
	vTaskDelay(pdMS_TO_TICKS(10000));
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
	vTaskDelay(pdMS_TO_TICKS(10000));
	while (1) {
		xReturn = xQueueReceive(ImageQueue, path, pdMS_TO_TICKS(3000));
		if (xReturn == pdTRUE) {
			if (xSemaphoreTake(xMutex, portMAX_DELAY) == pdTRUE) {
				ESP_LOGI("ImageTask", "Received image file: %s", path);
				lv_obj_t *img = lv_img_create(lv_screen_active());

				if (img != NULL) {
					bsp_display_lock(0);

					lv_obj_set_style_bg_color(lv_screen_active(),
											  lv_color_white(), LV_PART_MAIN);
					sprintf(image, "S:%s", path);
					lv_img_set_src(img, image); // 240 * 320

					lv_style_init(&style_max_height);
					lv_style_set_y(&style_max_height, 312);
					lv_obj_set_height(img, lv_pct(100));
					lv_obj_add_style(img, &style_max_height, LV_STATE_DEFAULT);

					lv_style_init(&style_max_width);
					lv_style_set_y(&style_max_width, 232);
					lv_obj_set_width(img, lv_pct(100));
					lv_obj_add_style(img, &style_max_height, LV_STATE_DEFAULT);

					lv_image_set_inner_align(img, LV_IMAGE_ALIGN_STRETCH);
					lv_obj_center(img);

					bsp_display_unlock();
				} else {
					ESP_LOGE(TAG, "Failed to create image for %s", path);
				}
				xSemaphoreGive(xMutex);
			}
			vTaskDelay(10);
		}
	}
}

// serivce lvgl events time requirements
uint32_t milliseconds() {
    int64_t v64 = esp_timer_get_time();
    return (v64 / 1000);
}

void app_main(void)
{
	static lv_obj_t *screen = NULL;

	vTaskDelay(pdMS_TO_TICKS(4000));
	xMutex = xSemaphoreCreateMutex();

	ESP_LOGI(TAG, "[APP] Startup..");
    ESP_LOGI(TAG, "[APP] Free memory: %" PRIu32 " bytes", esp_get_free_heap_size());

    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("transport", ESP_LOG_VERBOSE);

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(example_connect());

	imageQueue = xQueueCreate(8, 256);
	urlQueue = xQueueCreate(4, 256);
	if (imageQueue != NULL) {
		xTaskCreatePinnedToCore(vImageTask, "ImageTask", 6144, imageQueue, 16, NULL, tskNO_AFFINITY);
		xTaskCreatePinnedToCore(vURLTask, "vURLTask", 6144, urlQueue, 4, NULL, tskNO_AFFINITY);
	} else {
		ESP_LOGE(TAG, "Display Queues Failed.");
	}
	
    /* Initialize display and LVGL */
    bsp_display_cfg_t cfg = {
		.lvgl_port_cfg = ESP_LVGL_PORT_INIT_CONFIG(),
        .buffer_size = BSP_LCD_H_RES * 100, // BSP_LCD_V_RES, 
        .double_buffer = false,
        .flags = {
			.buff_dma = true,
            .buff_spiram = false,
        }
    };
    lv_display_t *disp = bsp_display_start_with_config(&cfg);
	
    /* Set display brightness to 100% */
    bsp_display_backlight_on();
	bsp_display_brightness_set(100);
    
	// #define LV_USE_SJPG 1
	// lv_split_jpeg_init();
    esp_lv_decoder_handle_t decoder_handle = NULL;
    esp_lv_decoder_init(&decoder_handle); //Initialize this after lvgl starts
    lv_tick_set_cb(milliseconds);
	
	/* Mount SPIFFS */
	bsp_spiffs_mount();
	
	bsp_display_lock(0);
	screen = lv_disp_get_scr_act(disp);
	
	ui_skoona_page(screen);
	
	bsp_display_unlock();
    bsp_display_backlight_on();
	
	/* Initialize all available buttons */
	button_handle_t btns[6] = {NULL};
	bsp_iot_button_create(btns, NULL, 6);

	/* Register a callback for button press */
	for (int i = 0; i < 5; i++) {
		iot_button_register_cb(btns[i], BUTTON_PRESS_DOWN, NULL, btn_handler, (void *) i);
	}

	// Start the HTTP server
	start_http_listener();
}
