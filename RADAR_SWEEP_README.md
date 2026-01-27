# Radar Sweep Display - Visual Radar Screen

LVGL v9.4 radar sweep visualization for `radar_target_t` structure on 320x240 screen.

## Visual Features

### Radar Display
- **Sweep Arc**: ±60° coverage (120° total field of view)
- **Range**: 8 meters (displayed with 180-pixel radius)
- **Position**: Radar origin at bottom-center of screen (160, 220)
- **Sweep Line**: Animated green line sweeping left-to-right-to-left
- **Shadow Trail**: Fading shadow effect behind sweep line
- **Range Rings**: Concentric circles at 2m, 4m, 6m, 8m
- **Angle Markers**: Lines at -60°, 0°, +60°
- **Range Labels**: Distance markers on display

### Target Display
- **People Markers**: Person symbols (👤) for each detected target
- **Color Coding by Distance**:
  - **Red**: Close range (0-2.4m) - Warning!
  - **Orange**: Medium range (2.4-4.8m) - Caution
  - **Green**: Far range (4.8-8m) - Safe
- **Target Labels**: Small "T0", "T1", "T2" labels below markers
- **Position**: Calculated from polar coordinates (distance, angle)

### Animation
- **Sweep Speed**: 3° per frame at 20 FPS (50ms timer)
- **Auto-reverse**: Bounces at ±60° limits
- **Shadow Width**: 30° fade trail
- **Smooth Motion**: Continuous sweep animation

## Files Created

1. **ui_radar_sweep.c** - Radar sweep implementation
2. **ui_radar_sweep.h** - Header file
3. **ui_radar_integration.c** - Integration helper and mode switching

## API Functions

### 1. Create Radar UI
```c
void radar_sweep_create_ui(lv_obj_t *parent);
```
Creates the radar display and starts sweep animation automatically.

### 2. Update Target Position
```c
void radar_sweep_update(radar_target_t *targets, int targetId, bool hasMoved);
```
Updates a single target's position on the radar display.

**Example:**
```c
bsp_display_lock(0);
radar_sweep_update(targets, 0, true);  // Update target 0
bsp_display_unlock();
```

### 3. Update Info Display
```c
void radar_sweep_update_info(int target_count);
```
Updates the header with current target count.

### 4. Animation Control
```c
void radar_sweep_stop_animation(void);   // Pause sweep
void radar_sweep_start_animation(void);  // Resume sweep
```

### 5. Cleanup
```c
void radar_sweep_delete_ui(void);
```
Stops animation and removes all UI elements.

## Coordinate System

### Polar to Cartesian Conversion
The radar uses polar coordinates from the sensor:
- **Distance**: 0-8000mm (normalized to 0-180 pixels)
- **Angle**: -60° to +60° (0° = forward, + = right, - = left)

Conversion formula:
```c
normalized_distance = (distance_mm / 8000.0) * 180
x_screen = 160 + normalized_distance * sin(angle_rad)
y_screen = 220 - normalized_distance * cos(angle_rad)
```

### Screen Layout
```
     0,0                                319,0
      ┌──────────────────────────────────┐
      │      Radar: 8m / ±60°            │ ← Info label
      │                                  │
      │         8m range ring            │
      │        6m range ring             │
      │       4m range ring              │
      │      2m range ring               │
      │         👤 T0                    │ ← Target markers
      │                                  │
      │   -60°    0°    +60°             │
      └─────────●──────────────────────── ┘
     0,240     160,220               319,240
                 ↑
           Radar origin
```

## Integration Steps

### Step 1: Update CMakeLists.txt

Add both new files to your main component:

```cmake
idf_component_register(
    SRCS
        "main.c"
        "mmwave.c"
        "ui_page01.c"
        "ui_radar_display.c"
        "ui_radar_sweep.c"        # Add this
        "ui_radar_integration.c"  # Add this
    INCLUDE_DIRS "."
)
```

### Step 2: Choose Integration Approach

#### Option A: Sweep Display Only

In `main.c`:
```c
#include "ui_radar_sweep.h"

void app_main(void) {
    // ... initialization ...

    bsp_display_lock(0);
    screen = lv_disp_get_scr_act(disp);
    radar_sweep_create_ui(screen);  // Use sweep display
    bsp_display_unlock();

    start_mmwave(NULL);
}
```

In `mmwave.c`:
```c
#include "bsp/esp-bsp.h"
#include "ui_radar_sweep.h"

void vRadarTask(void *pvParameters) {
    // ... initialization ...

    while (1) {
        if (radar_sensor_update(&radar)) {
            target_count = radar_sensor_get_targets(&radar, targets);

            // Update each target
            for (int idx = 0; idx < RADAR_MAX_TARGETS; idx++) {
                if (targets[idx].detected) {
                    bool hasMoved = /* check if moved */;

                    bsp_display_lock(0);
                    radar_sweep_update(targets, idx, hasMoved);
                    bsp_display_unlock();
                }
            }

            // Update target count
            bsp_display_lock(0);
            radar_sweep_update_info(target_count);
            bsp_display_unlock();
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
```

#### Option B: Both Displays with Button Switching

In `main.c`:
```c
#include "ui_radar_display.h"
#include "ui_radar_sweep.h"

// Global display reference
static lv_display_t *g_disp = NULL;

void btn_handler(void *button_handle, void *usr_data) {
    int button_index = (int)usr_data;

    if (button_index == 0) {
        // Button 0: Switch between list and sweep views
        radar_switch_display_mode(g_disp);
    }
}

void app_main(void) {
    // ... initialization ...

    g_disp = bsp_display_start_with_config(&cfg);

    // Start in sweep mode
    radar_display_init(g_disp, DISPLAY_MODE_SWEEP);

    start_mmwave(NULL);
}
```

In `mmwave.c`:
```c
#include "ui_radar_integration.h"

void vRadarTask(void *pvParameters) {
    // ... initialization ...

    while (1) {
        if (radar_sensor_update(&radar)) {
            target_count = radar_sensor_get_targets(&radar, targets);

            for (int idx = 0; idx < RADAR_MAX_TARGETS; idx++) {
                if (targets[idx].detected) {
                    bool hasMoved = /* check if moved */;

                    // Updates whichever display is active
                    radar_update_current_display(targets, idx, hasMoved);
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
```

## Performance Considerations

### Memory Usage
- **Canvas Buffer**: ~150KB (320x240x2 bytes)
- **UI Objects**: ~15 LVGL objects (~3-5KB)
- **Total**: ~155KB
- **Recommendation**: Use PSRAM for canvas buffer

### CPU Usage
- **Sweep Animation**: 20 FPS = 50ms per frame
- **Frame Time**: ~10-15ms per frame (drawing + update)
- **Impact**: <5% CPU usage on ESP32-S3

### Optimization Tips
1. **Reduce Sweep Speed**: Change `SWEEP_SPEED` to 2 or 1
2. **Lower FPS**: Change timer from 50ms to 100ms
3. **Simplify Shadow**: Reduce `shadow_arc_width` from 30 to 15
4. **Static Background**: Redraw background only when needed

## Customization

### Adjust Radar Parameters

```c
// In ui_radar_sweep.c, change these defines:
#define RADAR_CENTER_X 160        // Horizontal center
#define RADAR_CENTER_Y 220        // Vertical position (bottom)
#define RADAR_MAX_RANGE 8000.0f   // Max range in mm
#define RADAR_RADIUS 180          // Display radius in pixels
#define RADAR_SWEEP_ANGLE 60      // ±degrees
#define SWEEP_SPEED 3             // Degrees per frame
```

### Change Colors

```c
// In draw_radar_background():
arc_dsc.color = lv_color_hex(0x00FF00);  // Radar arc color
line_dsc.color = lv_color_hex(0x004400); // Angle lines
label_dsc.color = lv_color_hex(0x00AA00);// Range labels

// In draw_sweep_line():
line_dsc.color = lv_color_hex(0x00FF00);  // Sweep line

// In radar_sweep_update() - target colors:
lv_color_hex(0xFF0000)  // Close - red
lv_color_hex(0xFFAA00)  // Medium - orange
lv_color_hex(0x00FF00)  // Far - green
```

### Custom Target Markers

Replace the person symbol with custom images:

```c
// Instead of LV_SYMBOL_USER, use custom image:
lv_img_set_src(ui.target_markers[i], "S:/spiffs/target_icon.png");
```

## Troubleshooting

### Canvas Not Displaying
- Check PSRAM is enabled: `idf.py menuconfig` → Component config → ESP PSRAM
- Verify canvas buffer allocation
- Ensure `lv_canvas_set_buffer()` is called

### Sweep Animation Stutters
- Reduce timer frequency: 50ms → 100ms
- Simplify shadow drawing
- Check for other high-priority tasks blocking

### Targets Not Appearing
- Verify angle is within ±60°
- Check distance is within 8000mm
- Ensure `hasMoved` is true on first detection
- Check target coordinates with ESP_LOGI

### Memory Issues
```c
// Check heap before creating UI:
ESP_LOGI(TAG, "Free heap: %lu", esp_get_free_heap_size());
radar_sweep_create_ui(screen);
ESP_LOGI(TAG, "Free heap after UI: %lu", esp_get_free_heap_size());
```

## Visual Example

When running with 2 targets detected:

```
     ┌──────────────────────────────────┐
     │    Radar: 8m / ±60° | Targets: 2│
     │                                  │
     │          ···· 8m                 │
     │        ····  ····                │
     │      ····      ····  👤 T1       │
     │    ····   👤 T0  ····            │
     │   /     ····    ····   \         │
     │  /    ····        ····  \        │
     │ /   ····            ····  \      │
     │/  ····                ····  \    │
     └──●──────────────────────────────┘
       ↑
   Sweep line animating left-right
   with green shadow trail
```

## Comparison: List vs Sweep Display

| Feature | List Display | Sweep Display |
|---------|-------------|---------------|
| View Type | Numerical data | Visual/spatial |
| Target Info | Detailed (X,Y,D,A,S) | Position only |
| Animation | None | Sweep line |
| Best For | Precise values | Spatial awareness |
| CPU Usage | Low | Medium |
| Memory | ~3KB | ~155KB |
| Update Rate | Any | Sync with sweep |

## License

Use freely in your project.
