# Quick Start - Radar Display Integration

Complete guide to integrate the radar displays into your M5Stack project.

## Files Overview

### Display Files (Created)
```
main/
├── ui_radar_display.c/.h     - List view with detailed target data
├── ui_radar_sweep.c/.h        - Visual radar sweep with animation
└── ui_radar_integration.c/.h  - Display switching and helpers
```

### Documentation
```
RADAR_DISPLAY_README.md  - List display documentation
RADAR_SWEEP_README.md    - Sweep display documentation
QUICK_START.md          - This file
```

## 5-Minute Integration

### Step 1: Update CMakeLists.txt

Edit `/main/CMakeLists.txt`:

```cmake
idf_component_register(
    SRCS
        "main.c"
        "mmwave.c"
        "ui_page01.c"
        "ui_radar_display.c"      # Add
        "ui_radar_sweep.c"         # Add
        "ui_radar_integration.c"   # Add
    INCLUDE_DIRS "."
)
```

### Step 2: Modify main.c

Add at the top:
```c
#include "ui_radar_integration.h"

// Add global variable
static lv_display_t *g_disp = NULL;
```

Modify button handler:
```c
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
        // Button 2: Switch between list and sweep views
        radar_switch_display_mode(g_disp);
        ESP_LOGI(TAG, "Display mode switched");
        break;
    }
    ESP_LOGI(TAG, "Button %d pressed", button_index);
}
```

Modify app_main display initialization:
```c
void app_main(void)
{
    lv_obj_t *screen = NULL;

    logMemoryStats("App Main started");

    // ... existing initialization (nvs, netif, etc.) ...

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
    g_disp = bsp_display_start_with_config(&cfg);  // Save display handle

    ESP_ERROR_CHECK(example_connect());

    /* Set display brightness to 100% */
    bsp_display_backlight_on();
    bsp_display_brightness_set(100);

    esp_lv_decoder_handle_t decoder_handle = NULL;
    esp_lv_decoder_init(&decoder_handle);
    lv_tick_set_cb(milliseconds);

    /* Mount SPIFFS */
    bsp_spiffs_mount();

    /* Initialize buttons */
#define BUTTON_NUM 3
    button_handle_t btns[BUTTON_NUM] = {NULL};
    bsp_iot_button_create(btns, NULL, BUTTON_NUM);

    for (int i = 0; i < BUTTON_NUM; i++) {
        iot_button_register_cb(btns[i], BUTTON_PRESS_DOWN, NULL, btn_handler, (void *) i);
    }

    // show logo screen first
    bsp_display_lock(0);
		screen = lv_obj_create(g_disp);
		lv_scr_load(screen);
		ui_skoona_page(screen);
	bsp_display_unlock();

    start_mmwave(g_disp); // pass g_disp to mmwave
    logMemoryStats("App Main startup complete");
}
```

### Step 3: Modify mmwave.c

Add at the top:
```c
#include "bsp/esp-bsp.h"
#include "ui_radar_integration.h"
#include <math.h>
```

Modify vRadarTask:
```c
void vRadarTask(void *pvParameters) {
    radar_sensor_t radar;
    radar_target_t targets[RADAR_MAX_TARGETS];
    radar_target_t priorTargets[RADAR_MAX_TARGETS] = {0};

	while (!logoDone) {
		vTaskDelay(pdMS_TO_TICKS(1000)); // wait for logo screen to finish
	}

	// Initialize radar display (starts in SWEEP mode)
	radar_display_init((lv_display_t *)pvParameters, DISPLAY_MODE_SWEEP);

    // Initialize radar sensor
    esp_err_t ret = radar_sensor_init(&radar, CONFIG_UART_PORT,
                                      CONFIG_UART_RX_GPIO,
                                      CONFIG_UART_TX_GPIO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Radar initialization failed: %s", esp_err_to_name(ret));
        vTaskDelete(NULL);
    }

    ret = radar_sensor_begin(&radar, CONFIG_UART_SPEED_BPS);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start radar sensor");
        vTaskDelete(NULL);
    }

    // Configure radar
    radar_sensor_set_multi_target_mode(&radar, true);
    radar_sensor_set_retention_times(&radar, 10000, 500);

    ESP_LOGI(TAG, "Sensor is active, starting main loop.");

    int target_count = 0;

    while (1) {
        if (radar_sensor_update(&radar)) {
            // Save previous state
            memcpy(priorTargets, targets, sizeof(targets));

            // Get current targets
            target_count = radar_sensor_get_targets(&radar, targets);

            // Update each target
            for (int idx = 0; idx < RADAR_MAX_TARGETS; idx++) {
                bool hasMoved = false;

                if (targets[idx].detected) {
                    // Check if target has moved significantly (>50mm in any direction)
                    if (!priorTargets[idx].detected ||
                        fabsf(targets[idx].x - priorTargets[idx].x) > 50.0f ||
                        fabsf(targets[idx].y - priorTargets[idx].y) > 50.0f) {
                        hasMoved = true;
                    }

                    // Update display
                    radar_update_current_display(targets, idx, hasMoved);

                    // Log movement
                    if (hasMoved) {
                        ESP_LOGI(TAG, "[%d] X:%.0f Y:%.0f D:%.0f A:%.1f S:%.0f %s",
                                idx,
                                targets[idx].x,
                                targets[idx].y,
                                targets[idx].distance,
                                targets[idx].angle,
                                targets[idx].speed,
                                targets[idx].position_description);
                    }
                } else if (priorTargets[idx].detected) {
                    // Target lost
                    radar_update_current_display(targets, idx, false);
                    ESP_LOGI(TAG, "[%d] Target lost", idx);
                }
            }

            // Update target count info (for sweep display)
            if (radar_get_display_mode() == DISPLAY_MODE_SWEEP) {
                bsp_display_lock(0);
                radar_sweep_update_info(target_count);
                bsp_display_unlock();
            }
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
```

### Step 4: Build and Flash

```bash
cd /Users/jscott/Documents/PlatformIO/Projects/M5Stack/humanRadar
pio run -t upload
pio device monitor
```

## Usage

### Button Controls
- **Button 0 (Left)**: Log memory stats
- **Button 1 (Middle)**: Log memory stats
- **Button 2 (Right)**: Switch between List and Sweep displays

### Display Modes
### Logo Display
Display Skoona LLC logo animation

#### List Display Mode
Shows detailed numerical data for each target:
- Target ID and detection status
- X, Y coordinates (mm)
- Distance, angle, speed
- Position description

#### Sweep Display Mode
Shows visual radar with animated sweep:
- Radar arc with ±60° coverage
- Animated sweep line with shadow
- Target markers as person symbols
- Color-coded by distance (red=close, green=far)

## Verification

### Expected Console Output
```
I (1234) skoona.net: [APP] App Main started...
I (1456) RadarIntegration: Initialized in SWEEP mode
I (2000) mmWave: Sensor is active, starting main loop.
I (2500) mmWave: [0] X:1250 Y:850 D:1500 A:32.5 S:120 Front-Right, Close Range
I (3000) RadarSweep: Target 0 at screen pos (185, 145), distance 1500mm, angle 32.5°
```

### Visual Check
1. Screen should show animated radar sweep
2. Green line sweeps back and forth
3. When target detected, person symbol appears
4. Press Button 0 to switch to list view
5. List view shows detailed target data

## Troubleshooting

### Compile Errors

**Error: Cannot find ui_radar_integration.h**
- Check CMakeLists.txt includes all .c files
- Verify files are in `main/` directory
- Clean and rebuild: `pio run -t clean && pio run`

**Error: Undefined reference to radar_display_init**
- Ensure ui_radar_integration.c is in CMakeLists.txt SRCS
- Rebuild project

### Runtime Issues

**Display stays black**
- Check LVGL initialization completed
- Verify display brightness: `bsp_display_brightness_set(100)`
- Check logs for initialization errors

**Sweep animation not visible**
- Ensure canvas buffer allocated (needs PSRAM)
- Check `lv_tick_set_cb(milliseconds)` is called
- Verify timer is running (check logs)

**Targets not appearing**
- Verify radar sensor is connected and working
- Check console for target detection logs
- Ensure angles are within ±60°
- Verify distance is within 8000mm (8m)

**Button doesn't switch displays**
- Check g_disp is not NULL
- Verify button handler is registered
- Check logs when button pressed

### Memory Issues

Check heap usage:
```c
ESP_LOGI(TAG, "Free heap: %lu", esp_get_free_heap_size());
ESP_LOGI(TAG, "PSRAM free: %lu",
         esp_get_free_heap_size() - esp_get_free_internal_heap_size());
```

Canvas needs ~150KB. Ensure PSRAM is enabled:
```bash
idf.py menuconfig
# → Component config → ESP PSRAM → Enable PSRAM
```

## Performance Tips

### Reduce CPU Usage
In `ui_radar_sweep.c`:
```c
#define SWEEP_SPEED 2  // Slower sweep (was 3)

// In radar_sweep_create_ui():
sweep_timer = lv_timer_create(sweep_timer_cb, 100, NULL);  // 10 FPS (was 50ms)
```

### Reduce Memory Usage
Use smaller canvas buffer:
```c
#define RADAR_RADIUS 150  // Smaller display (was 180)
```

### Optimize Updates
Only update when targets move significantly:
```c
// In vRadarTask:
if (fabsf(targets[idx].x - priorTargets[idx].x) > 100.0f) {  // 100mm threshold
    radar_update_current_display(targets, idx, true);
}
```

## Next Steps

1. **Customize colors**: Edit color hex codes in ui_radar_sweep.c
2. **Adjust radar range**: Change `RADAR_MAX_RANGE` for different scales
3. **Add more displays**: Create additional visualization modes
4. **Network features**: Send radar data over WiFi/MQTT
5. **Data logging**: Log target tracks to SD card

## Support

For issues or questions:
- Check the detailed READMEs: RADAR_DISPLAY_README.md, RADAR_SWEEP_README.md
- Review example code in ui_radar_integration.c
- Enable debug logging: `esp_log_level_set("*", ESP_LOG_DEBUG);`

## Summary

You now have:
- ✅ Two radar display modes (List and Sweep)
- ✅ Animated visual radar with sweep line
- ✅ Button to switch between displays
- ✅ Color-coded target markers
- ✅ Real-time target tracking

Press Button 0 to switch modes and watch your radar in action!
