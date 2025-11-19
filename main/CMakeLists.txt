set(SOURCES main.c ui_page01.c http_listener.c http_requester.c )
set(LIBS nvs_flash esp_netif esp-tls esp_event esp_wifi esp_http_server esp_http_client spiffs esp_timer)
idf_component_register(
    SRCS ${SOURCES}
	PRIV_REQUIRES ${LIBS}
    INCLUDE_DIRS ".")

