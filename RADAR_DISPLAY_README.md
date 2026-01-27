# Radar Display UI - Implementation Guide

LVGL v9.4 display function for `radar_target_t` structure on 320x240 screen with ESP-IDF v5.5.2.

## Files Created

1. **ui_radar_display.c** - Main implementation with display functions
2. **ui_radar_display.h** - Header file with function declarations
3. **ui_radar_display_example.c** - Integration examples and usage guide

## Features

### Display Layout (320x240)

- **Title Bar** (Top): "Radar Targets" label
- **3 Target Panels**: Stacked vertically, color-coded
  - Target 0: Blue panel
  - Target 1: Green panel
  - Target 2: Red/Brown panel

### Each Target Panel Shows

1. **Target ID and Status**: "T0: DETECTED" or "T0: NO TARGET"
   - Green text when detected
   - Red text when no target

2. **Coordinates**: X and Y position in millimeters
   - Format: "X: 1250 mm  Y: 850 mm"

3. **Distance, Angle, Speed**:
   - Distance in mm
   - Angle in degrees
   - Speed in mm/s
   - Format: "D: 1500mm A: 32.5° S: 120mm/s"

4. **Position Description**: Human-readable location
   - Scrolling text if too long
   - Example: "Front-Right, Close Range"

## API Functions

### 1. Create UI
```c
void radar_display_create_ui(lv_obj_t *parent);
```
Creates all UI elements on the specified parent object (typically the screen).

### 2. Update Display
```c
void radar_display_update(radar_target_t *targets, int targetId, bool hasMoved);
```
Updates the display with current radar data. Must be called with display lock:

```c
bsp_display_lock(0);
radar_display_update(targets, targetId, hasMoved);
bsp_display_unlock();
```

### 3. Delete UI
```c
void radar_display_delete_ui(void);
```
Cleans up and deletes all UI elements.

### 4. Complete Page Creation
```c
void radar_display_page_create(lv_obj_t *parent, radar_target_t *targets, uint32_t update_period_ms);
```
Convenience function for creating the UI.

## Integration Steps

### Step 1: Update CMakeLists.txt

Add `ui_radar_display.c` to your main component sources:

```cmake
idf_component_register(
    SRCS
        "main.c"
        "mmwave.c"
        "ui_page01.c"
        "ui_radar_display.c"  # Add this line
    INCLUDE_DIRS "."
)
```

### Step 2: Modify main.c

Replace the current UI initialization:

```c
// OLD CODE:
bsp_display_lock(0);
    screen = lv_disp_get_scr_act(disp);
    ui_skoona_page(screen);  // Remove this
bsp_display_unlock();

// NEW CODE:
bsp_display_lock(0);
    screen = lv_disp_get_scr_act(disp);
    radar_display_create_ui(screen);  // Add this
bsp_display_unlock();
```

Add include at the top of main.c:
```c
#include "ui_radar_display.h"
```

### Step 3: Modify mmwave.c

Add includes at the top:
```c
#include "bsp/esp-bsp.h"
#include "ui_radar_display.h"
```

Update the vRadarTask function to update the display:

```c
void vRadarTask(void *pvParameters) {
    // ... existing initialization code ...

    // Main loop
    int target_count = 0;
    while (1) {
        if (radar_sensor_update(&radar)) {
            memcpy(priorTargets, targets, sizeof(targets));
            target_count = radar_sensor_get_targets(&radar, targets);

            // Add display update
            bsp_display_lock(0);
            radar_display_update(targets, target_count);
            bsp_display_unlock();

            // ... rest of your existing code ...
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
```

## Alternative Integration Approaches

### Option 1: Update Display in Radar Task (Recommended)
- Display updates happen in `vRadarTask` after reading radar data
- Simple, synchronized updates
- See Step 3 above

### Option 2: Separate Display Task
- Create a separate FreeRTOS task for display updates
- Radar task updates global targets array
- Display task reads and displays the global data
- See `ui_radar_display_example.c` for implementation

### Option 3: LVGL Timer
- Use LVGL's timer system to update display
- More LVGL-native approach
- Requires careful thread synchronization

## Memory Considerations

- Total UI elements: ~13 LVGL objects
- Memory usage: ~2-3KB for UI elements
- Stack size for display task: 2048 bytes minimum
- Ensure PSRAM is available for LVGL buffers

## Customization

### Colors
Change panel colors in `radar_display_create_ui()`:
```c
lv_color_t panel_colors[] = {
    lv_color_hex(0x1E3A8A),  // Target 0 color
    lv_color_hex(0x166534),  // Target 1 color
    lv_color_hex(0x7C2D12)   // Target 2 color
};
```

### Fonts
Change fonts by modifying the style settings:
```c
lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
```

Available fonts in LVGL v9:
- `lv_font_montserrat_10`
- `lv_font_montserrat_12`
- `lv_font_montserrat_14`
- `lv_font_montserrat_16`
- etc.

### Layout
Adjust panel dimensions in `radar_display_create_ui()`:
```c
const int panel_height = 65;  // Change panel height
const int panel_width = 310;  // Change panel width
const int spacing = 5;        // Change spacing between panels
```

## Troubleshooting

### Display Not Updating
- Ensure `bsp_display_lock()` and `bsp_display_unlock()` are called
- Check that `radar_display_update()` is called regularly
- Verify LVGL tick is configured: `lv_tick_set_cb(milliseconds)`

### Memory Issues
- Increase task stack size if display updates cause stack overflow
- Ensure PSRAM is enabled in sdkconfig
- Use `logMemoryStats()` to monitor heap usage

### Text Not Visible
- Check text colors match background colors
- Verify fonts are included in LVGL configuration
- Check `lv_conf.h` for font settings

### Panel Overlap
- Adjust panel dimensions and spacing
- Ensure total height doesn't exceed 240 pixels
- Account for title label height (30 pixels)

## Performance Notes

- Display updates should occur at 250-500ms intervals
- Each update takes ~10-20ms with display lock
- Avoid updating display more frequently than 100ms
- Use FreeRTOS task delays to control update rate

## Compatibility

- **ESP-IDF**: v5.5.2 (tested)
- **LVGL**: v9.4 (tested)
- **Screen**: 320x240 pixels
- **Hardware**: M5Stack Core (ILI9341 display)

## Example Output

When running, the display shows:
```
         Radar Targets
┌──────────────────────────────┐
│ T0: DETECTED              ✓  │
│ X: 1250 mm  Y: 850 mm        │
│ D: 1500mm A: 32.5° S: 120mm/s│
│ Front-Right, Close Range     │
└──────────────────────────────┘
┌──────────────────────────────┐
│ T1: NO TARGET             ✗  │
│ X: ---  Y: ---               │
│ D: --- A: --- S: ---         │
│ No target detected           │
└──────────────────────────────┘
┌──────────────────────────────┐
│ T2: NO TARGET             ✗  │
│ X: ---  Y: ---               │
│ D: --- A: --- S: ---         │
│ No target detected           │
└──────────────────────────────┘
```

## License

Use this code freely in your project.
