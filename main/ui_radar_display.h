/*
 * ui_radar_display.h
 * LVGL display function for radar_target_t structure
 * Screen: 320x240 pixels
 * ESP-IDF v5.5.2, LVGL v9.4
 */

#pragma once

#include "lvgl.h"
#include "humanRadarRD_03D.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create the UI layout for radar target display
 *
 * Creates a display showing up to 3 radar targets with:
 * - Target ID and detection status
 * - X, Y coordinates in mm
 * - Distance, angle, and speed
 * - Position description
 *
 * @param parent Parent LVGL object (typically the screen)
 */
void radar_display_create_ui(lv_obj_t *parent);

/**
 * @brief Update the display with current radar target data
 *
 * This function should be called with bsp_display_lock/unlock:
 *
 * Example:
 *   bsp_display_lock(0);
 *   radar_display_update(targets, target_count);
 *   bsp_display_unlock();
 *
 * @param targets Array of radar_target_t structures
 * @param targetId Number of target in the array
 * @param hasMoved Boolean indicating if the target has moved
 */
void radar_display_update(radar_target_t *targets, int targetId, bool hasMoved);

/**
 * @brief Clean up and delete all UI elements
 *
 * This should be called before switching to another screen
 */
void radar_display_delete_ui(void);

/**
 * @brief Complete radar display page creation
 *
 * Convenience function that creates the UI.
 * You should call radar_display_update() from your radar task
 * to update the display with new data.
 *
 * @param parent Parent LVGL object (typically the screen)
 * @param targets Pointer to radar targets array (should remain valid)
 * @param update_period_ms Update period in milliseconds (for reference)
 */
void radar_display_page_create(lv_obj_t *parent, radar_target_t *targets, uint32_t update_period_ms);

#ifdef __cplusplus
}
#endif
