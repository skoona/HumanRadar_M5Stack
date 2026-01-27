/*
 * ui_radar_display.c
 * LVGL display function for radar_target_t structure
 * Screen: 320x240 pixels
 * ESP-IDF v5.5.2, LVGL v9.4
 */

#include "lvgl.h"
#include "humanRadarRD_03D.h"
#include <stdio.h>

// UI element handles
typedef struct {
    lv_obj_t *scr;
    lv_obj_t *title_label;
    lv_obj_t *target_panels[RADAR_MAX_TARGETS];
    lv_obj_t *target_labels[RADAR_MAX_TARGETS];
    lv_obj_t *coord_labels[RADAR_MAX_TARGETS];
    lv_obj_t *data_labels[RADAR_MAX_TARGETS];
    lv_obj_t *pos_labels[RADAR_MAX_TARGETS];
} radar_display_ui_t;

static radar_display_ui_t ui;

/**
 * @brief Create the UI layout for radar target display
 *
 * @param parent Parent LVGL object (typically the screen)
 */
void radar_display_create_ui(lv_obj_t *parent)
{
    ui.scr = parent;

    // Set background color
    lv_obj_set_style_bg_color(parent, lv_color_hex(0x000000), 0);

    // Create title label at top
    ui.title_label = lv_label_create(parent);
    lv_label_set_text(ui.title_label, "Radar Targets");
    lv_obj_set_style_text_color(ui.title_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(ui.title_label, &lv_font_montserrat_14, 0);
    lv_obj_align(ui.title_label, LV_ALIGN_TOP_MID, 0, 5);

    // Create panels for each target (3 targets, stacked vertically)
    // Panel dimensions: 310x65 each (with 5px spacing)
    const int panel_height = 65;
    const int panel_width = 310;
    const int start_y = 30;
    const int spacing = 5;

    lv_color_t panel_colors[] = {
        lv_color_hex(0x1E3A8A),  // Blue for target 0
        lv_color_hex(0x166534),  // Green for target 1
        lv_color_hex(0x7C2D12)   // Red/brown for target 2
    };

    for (int i = 0; i < RADAR_MAX_TARGETS; i++) {
        // Create container panel
        ui.target_panels[i] = lv_obj_create(parent);
        lv_obj_set_size(ui.target_panels[i], panel_width, panel_height);
        lv_obj_set_pos(ui.target_panels[i], 5, start_y + i * (panel_height + spacing));
        lv_obj_set_style_bg_color(ui.target_panels[i], panel_colors[i], 0);
        lv_obj_set_style_border_width(ui.target_panels[i], 2, 0);
        lv_obj_set_style_border_color(ui.target_panels[i], lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_radius(ui.target_panels[i], 5, 0);
        lv_obj_set_style_pad_all(ui.target_panels[i], 5, 0);

        // Target ID label (top-left)
        ui.target_labels[i] = lv_label_create(ui.target_panels[i]);
        lv_label_set_text_fmt(ui.target_labels[i], "T%d: NO DATA", i);
        lv_obj_set_style_text_color(ui.target_labels[i], lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_text_font(ui.target_labels[i], &lv_font_montserrat_12, 0);
        lv_obj_align(ui.target_labels[i], LV_ALIGN_TOP_LEFT, 0, 0);

        // Coordinates label (below target label)
        ui.coord_labels[i] = lv_label_create(ui.target_panels[i]);
        lv_label_set_text(ui.coord_labels[i], "X: --- Y: ---");
        lv_obj_set_style_text_color(ui.coord_labels[i], lv_color_hex(0xCCCCCC), 0);
        lv_obj_set_style_text_font(ui.coord_labels[i], &lv_font_montserrat_10, 0);
        lv_obj_align(ui.coord_labels[i], LV_ALIGN_TOP_LEFT, 0, 18);

        // Data label (distance, angle, speed)
        ui.data_labels[i] = lv_label_create(ui.target_panels[i]);
        lv_label_set_text(ui.data_labels[i], "D: --- A: --- S: ---");
        lv_obj_set_style_text_color(ui.data_labels[i], lv_color_hex(0xCCCCCC), 0);
        lv_obj_set_style_text_font(ui.data_labels[i], &lv_font_montserrat_10, 0);
        lv_obj_align(ui.data_labels[i], LV_ALIGN_TOP_LEFT, 0, 33);

        // Position description label (bottom)
        ui.pos_labels[i] = lv_label_create(ui.target_panels[i]);
        lv_label_set_text(ui.pos_labels[i], "---");
        lv_obj_set_style_text_color(ui.pos_labels[i], lv_color_hex(0xFFFF00), 0);
        lv_obj_set_style_text_font(ui.pos_labels[i], &lv_font_montserrat_10, 0);
        lv_obj_set_width(ui.pos_labels[i], panel_width - 20);
        lv_label_set_long_mode(ui.pos_labels[i], LV_LABEL_LONG_SCROLL_CIRCULAR);
        lv_obj_align(ui.pos_labels[i], LV_ALIGN_BOTTOM_LEFT, 0, 0);
    }
}

/**
 * @brief Update the display with current radar target data
 *
 * @param targets Array of radar_target_t structures
 * @param targetId Number of target in the array
 * @param hasMoved Boolean indicating if the target has moved
 */
void radar_display_update(radar_target_t *targets, int targetId, bool hasMoved) 
{
    if (targets == NULL) {
        return;
    }

	if (hasMoved || targets[targetId].detected) {
		// Target is detected - show all data

		// Update target status
		lv_label_set_text_fmt(ui.target_labels[targetId], "T%d: DETECTED", targetId);
		lv_obj_set_style_text_color(ui.target_labels[targetId], lv_color_hex(0x00FF00),
									0);

		// Update coordinates (in mm)
		lv_label_set_text_fmt(ui.coord_labels[targetId], "X: %.0f mm  Y: %.0f mm",
							  targets[targetId].x, targets[targetId].y);

		// Update distance, angle, speed
		lv_label_set_text_fmt(
			ui.data_labels[targetId], "D: %.0fmm A: %.1f° S: %.0fmm/s",
			targets[targetId].distance, targets[targetId].angle, targets[targetId].speed);

		// Update position description
		lv_label_set_text(ui.pos_labels[targetId], targets[targetId].position_description);

	} else {
		// No target detected
		lv_label_set_text_fmt(ui.target_labels[targetId], "T%d: NO TARGET", targetId);
		lv_obj_set_style_text_color(ui.target_labels[targetId], lv_color_hex(0xFF0000),	0);

		lv_label_set_text(ui.coord_labels[targetId], "X: ---  Y: ---");
		lv_label_set_text(ui.data_labels[targetId], "D: --- A: --- S: ---");
		lv_label_set_text(ui.pos_labels[targetId], "No target detected");
	}
}

/**
 * @brief Clean up and delete all UI elements
 *
 * This should be called before switching to another screen
 */
void radar_display_delete_ui(void)
{
    // Delete all created objects
    if (ui.title_label) {
        lv_obj_del(ui.title_label);
    }

    for (int i = 0; i < RADAR_MAX_TARGETS; i++) {
        if (ui.target_panels[i]) {
            lv_obj_del(ui.target_panels[i]);
        }
    }
}

/**
 * @brief Complete radar display page creation and update function
 *
 * This is a convenience function that creates the UI and sets up
 * a periodic timer to update the display
 *
 * @param parent Parent LVGL object (typically the screen)
 * @param targets Pointer to radar targets array (should remain valid)
 * @param update_period_ms Update period in milliseconds
 */
void radar_display_page_create(lv_obj_t *parent, radar_target_t *targets, uint32_t update_period_ms)
{
    radar_display_create_ui(parent);

    // Note: For automatic updates, you would typically create an LVGL timer here
    // that calls radar_display_update() periodically. However, since the targets
    // are updated in a FreeRTOS task, you should call radar_display_update()
    // from your radar task after acquiring the display lock.
    //
    // Example in your radar task:
    // bsp_display_lock(0);
    // radar_display_update(targets, target_count);
    // bsp_display_unlock();
}
