set(SOURCES main.c ui_page01.c mmwave.c ui_radar_display.c ui_radar_sweep.c ui_radar_integration.c)
set(LIBS nvs_flash esp_netif esp-tls esp_event esp_wifi spiffs esp_timer humanRadarRD_03D)
idf_component_register(
    SRCS ${SOURCES}
	PRIV_REQUIRES ${LIBS}
    INCLUDE_DIRS ".")

