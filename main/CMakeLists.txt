idf_component_register(SRCS "main.c" "ui_page01.c" "http_listener.c" "http_requester.c"
					PRIV_REQUIRES nvs_flash esp_netif esp-tls esp_event esp_wifi esp_http_server esp_http_client spiffs
                    INCLUDE_DIRS ".")

spiffs_create_partition_image(storage ../spiffs FLASH_IN_PROJECT)
