/* ESP HTTP Listener

*/

#include "esp_err.h"
#include "esp_heap_caps.h"
#include <esp_http_server.h>
#include <esp_log.h>
#include <nvs_flash.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cJSON.h>

static const char *TAG = "LISTENER";
extern esp_err_t handleAlarms(char *device);

esp_err_t processAlarmResponse(char *path, cJSON * root, char *target) {
	cJSON *alarm = NULL;
	cJSON *triggers = NULL;
	cJSON *elem = NULL;
	cJSON *device = NULL;

	alarm = cJSON_GetObjectItemCaseSensitive(root, "alarm");
	if (alarm == NULL) {
		ESP_LOGE(TAG, "Alarm element not found");
		return ESP_FAIL;
	}
	triggers = cJSON_GetObjectItemCaseSensitive(alarm, "triggers");
	if (triggers == NULL) {
		ESP_LOGE(TAG, "Triggers element not found");
		return ESP_FAIL;
	}	
	elem = cJSON_GetArrayItem(triggers, 0);
	if (elem == NULL) {
		ESP_LOGE(TAG, "First Element in triggers not found");
		return ESP_FAIL;
	}
	device = cJSON_GetObjectItemCaseSensitive(elem, "device");
	if (device == NULL || !cJSON_IsString(device) || (device->valuestring == NULL)) {
		ESP_LOGE(TAG, "Device element not found");
		return ESP_FAIL;
	}
	
	strcpy(target, device->valuestring);

	return ESP_OK;
}

esp_err_t handleWebhookResult(char *path, char *content, char *content_type, size_t bytes_received) {
	char device[32] = {'\0'};
	memset(device, 0,sizeof(device));

	cJSON *json = cJSON_Parse(content);
	if (json == NULL) {
		ESP_LOGI(TAG, "cJSON_Parse Failed: [L=%d]%s\n", bytes_received, content);
		return ESP_FAIL;
	}

	if (processAlarmResponse(path, json, device) == ESP_OK) {	
		cJSON_Delete(json);
		return handleAlarms(device);		
	} else {
		char *json_string = cJSON_Print(json);
		if (json_string == NULL) {
			ESP_LOGI(TAG, "cJSON_Print Failed: [L=%d]%s\n", bytes_received, content);
			cJSON_Delete(json);
			return ESP_FAIL;
		}
		cJSON_free(json_string);
	}
	cJSON_Delete(json);

	return ESP_OK;
}

// URI handler for the root path "/"
esp_err_t root_get_handler(httpd_req_t *req) {
	httpd_resp_set_status(req, "200 OK");
    const char* resp_str = "Hello from ESP32-S3!";
    httpd_resp_send(req, resp_str, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

/* URI handler for UniFi Webhook
 - LISTENER: Notification Received: 
	{
	“alarm”:	{
		“name”:	“Person”,
		“sources”:	[],
		“conditions”:	[{
				“condition”:	{
					“type”:	“is”,
					“source”:	“person”
				}
			}],
		“triggers”:	[{
				“key”:	“person”,
				“device”:	“70A7413F0FD7”,
				“zones”:	{
					“loiter”:	[],
					“zone”:	[],
					“line”:	[1]
				},
				“eventId”:	“690e50b200ba0503e40e9015”,
				“timestamp”:	1762545842310
			}, {
				“key”:	“person”,
				“device”:	“70A7413F0FD7”,  ---- MAC ADDRESS
				“zones”:	{
					“loiter”:	[],
					“zone”:	[1],
					“line”:	[]
				},
				“eventId”:	“690e50b2030c0503e40e9029”,
				“timestamp”:	1762545842808
			}],
		“eventPath”:	“/protect/events/event/690e50b200ba0503e40e9015”,
		“eventLocalLink”:	“https://10.100.1.1/protect/events/event/690e50b200ba0503e40e9015”
	},
	“timestamp”:	1762545843235
}
*/
esp_err_t unifi_cb(httpd_req_t *req) {
	char *content; 
    size_t bytes_received;
	char *path = "/spiffs/listener_event.jpg";

	// Get the content length from the request headers
    size_t content_len = req->content_len;

	content= calloc(content_len+4, sizeof(char));
 	// content = heap_caps_malloc(content_len+4, MALLOC_CAP_8BIT);
    if (content == NULL) { // Error or connection closed
        httpd_resp_send_500(req); // alloc error
        return ESP_FAIL;
    }

    // Read the request body into the buffer
    bytes_received = httpd_req_recv(req, content, content_len);
    if (bytes_received <= 0) { // Error or connection closed
        if (bytes_received == HTTPD_SOCK_ERR_TIMEOUT) {
            httpd_resp_send_408(req); // Request timeout
        }
        return ESP_FAIL;
    }

	// Might be binary image: check headers 
	// -- key=Content-Type, value=image/jpeg
	char content_type[32];
	httpd_req_get_hdr_value_str(req, "Content-Type", content_type, sizeof(content_type));

	// Null-terminate the received data for string manipulation
	content[bytes_received] = '\0';
	ESP_LOGI(TAG, "Content-Type: %s BytesReceived:%d Content:[%s]", content_type, bytes_received, content);

	esp_err_t ret = handleWebhookResult(path, content, content_type, bytes_received);
	
	free(content);

	// Send a response back to the client
	httpd_resp_set_status(req, "200 OK");
	httpd_resp_send(req, NULL, 0);

    return ret;
}


// Function to start the HTTP server
httpd_handle_t start_http_listener(void) {
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
	

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {

        // Register URI handlers
        httpd_uri_t root_uri = {
            .uri      = "/",
            .method   = HTTP_ANY,
            .handler  = root_get_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &root_uri);

		// Webhook for all notifications
        httpd_uri_t unifi_cb_uri = {
            .uri      = "/notify",
            .method   = HTTP_POST,
            .handler  = unifi_cb,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &unifi_cb_uri);

    }
    return server;
}

