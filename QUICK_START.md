# Quick Start - Radar Display Integration

Complete guide to integrate the radar displays into your M5Stack project.

## Files Overview

### Display Files (Created)
```
main/
├── ui_page01.c/.h             - Splash screen with Skoona logo animation
├── ui_radar_display.c/.h      - List view with detailed target data
├── ui_radar_sweep.c/.h        - Visual radar sweep with animation
└── ui_radar_integration.c/.h  - Display switching and helpers
```

### Documentation
```
RADAR_DISPLAY_README.md  - List display documentation
RADAR_SWEEP_README.md    - Sweep display documentation
QUICK_START.md          - This file
```

## Current Implementation

The project is already configured with:
- ✅ Splash screen on startup (Skoona logo animation)
- ✅ 4-second delay after splash screen
- ✅ Automatic transition to radar sweep display
- ✅ Button 0: Switch between List and Sweep displays
- ✅ Button 1: Log memory stats
- ✅ Button 2: Log memory stats

## Startup Sequence

1. **Display Initialization** - LVGL and hardware setup
2. **Splash Screen** - Shows Skoona logo with animated arcs (~4.4 seconds)
3. **4-Second Delay** - Extra time to view the completed logo
4. **Radar Display** - Transitions to radar sweep mode
5. **Sensor Start** - mmWave radar begins monitoring

## Current Code Structure

### main.c
```c
#include "ui_radar_integration.h"

extern void ui_skoona_page(lv_obj_t *scr);
extern void start_mmwave(void *pvParameters);

static const char *TAG = "skoona.net";
bool logoDone = false; // Control when radar screen is created
static lv_display_t *g_disp = NULL;

void btn_handler(void *button_handle, void *usr_data) {
    radar_btn_handler_example(button_handle, usr_data, g_disp);
}

void app_main(void)
{
    // ... initialization (nvs, netif, display, etc.) ...

    // Show splash screen with Skoona logo animation
    bsp_display_lock(0);
    lv_obj_t *screen = lv_disp_get_scr_act(g_disp);
    ui_skoona_page(screen);
    bsp_display_unlock();

    ESP_LOGI(TAG, "Splash screen displayed, waiting for animation to complete...");

    // Wait for logo animation to complete
    while (!logoDone) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    ESP_LOGI(TAG, "Animation complete, waiting 4 seconds...");
    vTaskDelay(pdMS_TO_TICKS(4000));

    // Clean up splash screen and initialize radar display
    bsp_display_lock(0);
    lv_obj_clean(screen);  // Remove all objects from screen
    bsp_display_unlock();

    ESP_LOGI(TAG, "Initializing radar display...");
    // Initialize radar display system (starts in SWEEP mode)
    radar_display_init(g_disp, DISPLAY_MODE_SWEEP);

    start_mmwave(NULL);
    logMemoryStats("App Main startup complete");
}
```

### ui_page01.c (Splash Screen)
```c
extern bool logoDone;

void anim_timer_cb(lv_timer_t *timer)
{
    // ... animation logic ...

    // Delete timer when all animation finished
    if ((count += 5) == 220) {
        lv_timer_del(timer);
        logoDone = true;  // Signal that animation is complete
    } else {
        timer_ctx->count_val = count;
    }
}

void ui_skoona_page(lv_obj_t *scr)
{
    vTaskDelay(pdMS_TO_TICKS(100)); // Small delay to ensure LVGL processes

    // Create animated logo and arcs
    // Animation runs for ~4.4 seconds
    // Sets logoDone = true when complete
}
```

### ui_radar_integration.c (Button Handler)
```c
void radar_btn_handler_example(void *button_handle, void *usr_data, lv_display_t *disp)
{
    int button_index = (int)usr_data;

    switch (button_index) {
    case 0:
        // Button 0: Switch display mode
        radar_switch_display_mode(disp);
        break;
    case 1:
        // Button 1: Log memory stats
        logMemoryStats("Button 1 pressed");
        break;
    case 2:
        // Button 2: Log memory stats
        logMemoryStats("Button 2 pressed");
        break;
    }
}
```

## Button Controls

- **Button 0 (Left)**: Switch between List and Sweep displays
- **Button 1 (Middle)**: Log memory statistics
- **Button 2 (Right)**: Log memory statistics

## Display Modes

### 1. Splash Screen (Startup Only)
- Skoona LLC logo with animated colored arcs
- Duration: ~4.4 seconds animation + 4 seconds delay
- Automatic transition to radar display

### 2. List Display Mode
Shows detailed numerical data for each target:
- Target ID and detection status (green/red)
- X, Y coordinates (mm)
- Distance, angle, speed
- Position description (scrolling text)

### 3. Sweep Display Mode (Default)
Shows visual radar with animated sweep:
- Radar arc with ±60° coverage
- 8-meter range with range rings at 2m intervals
- Animated sweep line with shadow trail
- Target markers as WiFi+P symbols
- Color-coded by distance:
  - Red: 0-2.4m (close)
  - Orange: 2.4-4.8m (medium)
  - Green: 4.8-8m (far)

## Expected Console Output

```
I (0) skoona.net: [APP] App Main started...
I (1234) skoona.net: Splash screen displayed, waiting for animation to complete...
I (5678) skoona.net: Animation complete, waiting 4 seconds...
I (9678) skoona.net: Initializing radar display...
I (9680) RadarIntegration: Initialized in SWEEP mode
I (10000) mmWave: Sensor is active, starting main loop.
I (10500) mmWave: [0] X:1250 Y:850 D:1500 A:32.5 S:120 Front-Right, Close Range
I (11000) RadarSweep: Target 0 at screen pos (185, 145), distance 1500mm, angle 32.5°
```

## Verification Steps

1. **Power on device** - Splash screen should appear immediately
2. **Watch animation** - Arcs spin around Skoona logo
3. **Wait for delay** - Logo stays visible for 4 extra seconds
4. **Radar appears** - Screen transitions to animated radar sweep
5. **Test buttons**:
   - Press Button 0 → Switches to list view
   - Press Button 0 again → Switches back to sweep view
   - Press Button 1 or 2 → Logs memory stats to console
6. **Watch for targets** - When person detected, symbol appears on radar

## Troubleshooting

### Compile Errors

**Error: Cannot find ui_radar_integration.h**
- Check CMakeLists.txt includes all .c files
- Verify files are in `main/` directory
- Clean and rebuild: `pio run -t clean && pio run`

**Error: Undefined reference to radar_display_init**
- Ensure ui_radar_integration.c is in CMakeLists.txt SRCS
- Rebuild project

**Error: lv_malloc undefined**
- Update to LVGL v9.4
- Check LVGL is properly configured in sdkconfig

### Runtime Issues

**Splash screen doesn't appear**
- Check SPIFFS is mounted: `bsp_spiffs_mount()`
- Verify logo images exist in `/spiffs/` directory
- Check logs for image loading errors

**Display stays black after splash**
- Check `logoDone` flag is being set correctly
- Verify `lv_obj_clean(screen)` is called
- Check logs for initialization errors

**Radar lines not visible**
- Issue: Static points array in `create_line()`
- Solution: Already fixed - uses `lv_malloc()` for persistent points
- Verify PSRAM is enabled for memory allocation

**Sweep animation not visible**
- Ensure `lv_tick_set_cb(milliseconds)` is called
- Check timer is running (50ms interval)
- Verify sweep_timer is not NULL

**Targets not appearing**
- Verify radar sensor is connected (UART)
- Check console for target detection logs
- Ensure angles are within ±60°
- Verify distance is within 8000mm (8m)
- Check `hasMoved` flag is true on first detection

**Button doesn't switch displays**
- Check g_disp is not NULL
- Verify button handler is registered
- Check logs when button pressed
- Ensure radar_display_init() completed successfully

### Memory Issues

Check heap usage:
```c
ESP_LOGI(TAG, "Free heap: %lu", esp_get_free_heap_size());
ESP_LOGI(TAG, "PSRAM free: %lu",
         esp_get_free_heap_size() - esp_get_free_internal_heap_size());
```

Memory requirements:
- Radar lines: ~300 LVGL objects (~60KB)
- Shadow lines: 30 LVGL objects (~6KB)
- Total UI: ~70-80KB

Ensure PSRAM is enabled:
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
Decrease number of line segments:
```c
// In create_radar_background():
for (int i = 0; i < 31; i++) {  // Was 61, now use 4° steps
    int angle = -60 + i * 4;
    // ...
}
```

### Optimize Updates
Only update when targets move significantly:
```c
// In vRadarTask (mmwave.c):
if (fabsf(targets[idx].x - priorTargets[idx].x) > 100.0f) {  // 100mm threshold
    radar_update_current_display(targets, idx, true);
}
```

## Customization

### Change Splash Screen Duration
In `main.c`:
```c
vTaskDelay(pdMS_TO_TICKS(4000));  // Change from 4000ms to desired delay
```

### Disable Splash Screen
In `main.c`, comment out the splash screen section:
```c
// Show splash screen (DISABLED)
// bsp_display_lock(0);
// lv_obj_t *screen = lv_disp_get_scr_act(g_disp);
// ui_skoona_page(screen);
// bsp_display_unlock();
// while (!logoDone) { vTaskDelay(pdMS_TO_TICKS(100)); }
// vTaskDelay(pdMS_TO_TICKS(4000));
// bsp_display_lock(0);
// lv_obj_clean(screen);
// bsp_display_unlock();

// Directly initialize radar display
radar_display_init(g_disp, DISPLAY_MODE_SWEEP);
```

### Change Target Marker Symbol
In `ui_radar_sweep.c`:
```c
lv_label_set_text(ui.target_markers[i], LV_SYMBOL_WIFI "P");  // Current
// Change to:
lv_label_set_text(ui.target_markers[i], LV_SYMBOL_USER);      // Person icon
lv_label_set_text(ui.target_markers[i], LV_SYMBOL_WARNING);   // Warning icon
lv_label_set_text(ui.target_markers[i], "●");                 // Circle
```

### Change Radar Colors
In `ui_radar_sweep.c`:
```c
// Outer arc
ui.arc_lines[i] = create_line(parent, x1, y1, x2, y2,
    lv_color_hex(0x00FF00), 2, LV_OPA_COVER);  // Green

// Range rings
ui.range_rings[ring][i] = create_line(parent, x1, y1, x2, y2,
    lv_color_hex(0x003300), 1, LV_OPA_COVER);  // Dark green

// Sweep line
ui.sweep_line = create_line(ui.radar_base, RADAR_CENTER_X, RADAR_CENTER_Y,
    RADAR_CENTER_X, RADAR_CENTER_Y - RADAR_RADIUS,
    lv_color_hex(0x00FF00), 2, LV_OPA_COVER);  // Bright green
```

### Change Default Display Mode
In `main.c`:
```c
// Start in LIST mode instead of SWEEP
radar_display_init(g_disp, DISPLAY_MODE_LIST);
```

## Architecture Overview

```
Startup Flow:
├── main.c (app_main)
│   ├── Initialize hardware
│   ├── ui_page01.c (ui_skoona_page)  [Splash Screen]
│   │   └── Animation timer (~4.4s)
│   ├── Wait for logoDone flag
│   ├── Delay 4 seconds
│   ├── Clean screen
│   └── ui_radar_integration.c (radar_display_init)
│       ├── DISPLAY_MODE_LIST → ui_radar_display.c
│       └── DISPLAY_MODE_SWEEP → ui_radar_sweep.c [Default]
│
├── Button Handler
│   └── radar_btn_handler_example
│       ├── Button 0 → radar_switch_display_mode()
│       ├── Button 1 → logMemoryStats()
│       └── Button 2 → logMemoryStats()
│
└── mmwave.c (vRadarTask)
    └── radar_update_current_display()
        ├── LIST mode → radar_display_update()
        └── SWEEP mode → radar_sweep_update()
```

## File Dependencies

```
main.c
├── #include "ui_radar_integration.h"
├── extern ui_skoona_page()
└── extern start_mmwave()

ui_radar_integration.c
├── #include "ui_radar_display.h"
└── #include "ui_radar_sweep.h"

ui_radar_sweep.c
├── #include "lvgl.h"
└── #include "humanRadarRD_03D.h"

ui_page01.c
├── #include "lvgl.h"
└── extern bool logoDone
```

## Support

For issues or questions:
- Check the detailed READMEs: RADAR_DISPLAY_README.md, RADAR_SWEEP_README.md
- Review example code in ui_radar_integration.c
- Enable debug logging: `esp_log_level_set("*", ESP_LOG_DEBUG);`

## Summary

Your system now features:
- ✅ Professional splash screen on startup
- ✅ Two radar display modes (List and Sweep)
- ✅ Animated visual radar with sweep line and shadow
- ✅ Button to switch between displays
- ✅ Color-coded target markers
- ✅ Real-time target tracking
- ✅ Memory-efficient line rendering with persistent points

Press Button 0 to switch modes and watch your radar in action!
