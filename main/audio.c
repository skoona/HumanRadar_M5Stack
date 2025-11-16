/*
 * WAV Audio Player
*/
#include <dirent.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "bsp/esp-bsp.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

/* Buffer for reading/writing to I2S driver. Same length as SPIFFS buffer and
   I2S buffer, for optimal read/write performance. Recording audio data path:
   I2S peripheral -> I2S buffer (DMA) -> App buffer (RAM) -> SPIFFS buffer ->
   External SPI Flash. Vice versa for playback. */
#define BUFFER_SIZE (1024)
#define SAMPLE_RATE (22050)
#define DEFAULT_VOLUME (20)

static const char *TAG = "AUDIO";
static esp_codec_dev_handle_t spk_codec_dev = NULL;

/*******************************************************************************
 * Types definitions
 *******************************************************************************/
typedef enum {
	APP_FILE_TYPE_UNKNOWN,
	APP_FILE_TYPE_TXT,
	APP_FILE_TYPE_IMG,
	APP_FILE_TYPE_WAV,
} app_file_type_t;

// Very simple WAV header, ignores most fields
typedef struct __attribute__((packed)) {
	uint8_t ignore_0[22];
	uint16_t num_channels;
	uint32_t sample_rate;
	uint8_t ignore_1[6];
	uint16_t bits_per_sample;
	uint8_t ignore_2[4];
	uint32_t data_size;
	uint8_t data[];
} dumb_wav_header_t;

/* Audio */
static SemaphoreHandle_t audio_mux;


static void play_file(void *arg) {
	char *path = arg;
	FILE *file = NULL;
	int16_t *wav_bytes = heap_caps_malloc(BUFFER_SIZE, MALLOC_CAP_DEFAULT);
	if (wav_bytes == NULL) {
		ESP_LOGE(TAG, "Not enough memory for playing!");
		goto END;
	}

	/* Open file */
	file = fopen(path, "rb");
	if (file == NULL) {
		ESP_LOGE(TAG, "%s file does not exist!", path);
		goto END;
	}

	/* Read WAV header file */
	dumb_wav_header_t wav_header;
	if (fread((void *)&wav_header, 1, sizeof(wav_header), file) !=
		sizeof(wav_header)) {
		ESP_LOGW(TAG, "Error in reading file");
		goto END;
	}
	ESP_LOGI(TAG, "Number of channels: %" PRIu16 "", wav_header.num_channels);
	ESP_LOGI(TAG, "Bits per sample: %" PRIu16 "", wav_header.bits_per_sample);
	ESP_LOGI(TAG, "Sample rate: %" PRIu32 "", wav_header.sample_rate);
	ESP_LOGI(TAG, "Data size: %" PRIu32 "", wav_header.data_size);

	esp_codec_dev_sample_info_t fs = {
		.sample_rate = wav_header.sample_rate,
		.channel = wav_header.num_channels,
		.bits_per_sample = wav_header.bits_per_sample,
		.mclk_multiple = I2S_MCLK_MULTIPLE_384,
	};
	esp_codec_dev_open(spk_codec_dev, &fs);

	uint32_t bytes_send_to_i2s = 0;

    bytes_send_to_i2s = 0;
    fseek(file, sizeof(wav_header), SEEK_SET);
    while (bytes_send_to_i2s < wav_header.data_size) {
        xSemaphoreTake(audio_mux, portMAX_DELAY);

        /* Get data from SPIFFS */
        size_t bytes_read_from_spiffs =
            fread(wav_bytes, 1, BUFFER_SIZE, file);

        /* Send it to I2S */
        esp_codec_dev_write(spk_codec_dev, wav_bytes,
                            bytes_read_from_spiffs);
        bytes_send_to_i2s += bytes_read_from_spiffs;
        xSemaphoreGive(audio_mux);
    }
    vTaskDelay(pdMS_TO_TICKS(100));

END:
	esp_codec_dev_close(spk_codec_dev);

	if (file) {
		fclose(file);
	}

	if (wav_bytes) {
		free(wav_bytes);
	}

	vTaskDelete(NULL);
}
void play_wave_file(char *path) {
	xTaskCreate(play_file, "audio_task", 4096, path, 6, NULL);
}

void app_audio_init(void) {
	audio_mux = xSemaphoreCreateMutex();
	assert(audio_mux);

	/* Initialize speaker */
	spk_codec_dev = bsp_audio_codec_speaker_init();
	assert(spk_codec_dev);
	/* Speaker output volume */
	esp_codec_dev_set_out_vol(spk_codec_dev, DEFAULT_VOLUME);

}
