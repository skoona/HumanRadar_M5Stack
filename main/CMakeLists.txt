idf_component_register(SRCS "main.c" "lvgl_demo_ui.c" "http_listener.c" "http_requester.c"
					PRIV_REQUIRES nvs_flash esp_netif esp-tls esp_event esp_wifi esp_http_server esp_http_client spiffs
                    INCLUDE_DIRS ".")

#lvgl_port_create_c_image("images/esp_logo.png" "images/gen/" "ARGB8888" "NONE")
#lvgl_port_create_c_image("images/esp_text.png" "images/gen/" "ARGB8888" "NONE")
#lvgl_port_add_images(${COMPONENT_LIB} "images/gen/")

spiffs_create_partition_image(storage ../spiffs FLASH_IN_PROJECT)

#spiffs_create_partition_image(
#    "storage"
#    "../spiffs"
#    FLASH_IN_PROJECT
#    MMAP_FILE_SUPPORT_FORMAT ".jpg,.png"
#    MMAP_SUPPORT_SJPG
#    MMAP_SPLIT_HEIGHT 16)
