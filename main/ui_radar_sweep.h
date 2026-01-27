/*
 * ui_radar_sweep.h
 * LVGL radar sweep display for radar_target_t structure
 * Screen: 320x240 pixels
 * Radar: ±60° sweep, 8 meters range
 * ESP-IDF v5.5.2, LVGL v9.4
 */

#pragma once

#include "lvgl.h"
#include "humanRadarRD_03D.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create the radar sweep UI
 *
 * Creates a visual radar display with:
 * - Animated sweep line with shadow trail
 * - ±60° sweep arc (120° total)
 * - 8 meter range with range rings at 2m intervals
 * - Angle markers at -60°, 0°, +60°
 * - People markers for detected targets
 *
 * The sweep line animates automatically at 20 FPS.
 *
 * @param parent Parent LVGL object (typically the screen)
 */
void radar_sweep_create_ui(lv_obj_t *parent);

/**
 * @brief Update target position on radar display
 *
 * Updates a single target's position on the radar.
 * Targets are shown as person symbols with color indicating distance:
 * - Red: Close range (0-2.4m)
 * - Orange: Medium range (2.4-4.8m)
 * - Green: Far range (4.8-8m)
 *
 * This function should be called with bsp_display_lock/unlock:
 *
 * Example:
 *   bsp_display_lock(0);
 *   radar_sweep_update(targets, targetId, hasMoved);
 *   bsp_display_unlock();
 *
 * @param targets Array of radar_target_t structures
 * @param targetId Target ID to update (0-2)
 * @param hasMoved Boolean indicating if the target has moved
 */
void radar_sweep_update(radar_target_t *targets, int targetId, bool hasMoved);

/**
 * @brief Update info label with target count
 *
 * @param target_count Number of detected targets
 */
void radar_sweep_update_info(int target_count);

/**
 * @brief Stop the sweep animation
 *
 * Freezes the sweep line at current position.
 * Useful for debugging or saving power.
 */
void radar_sweep_stop_animation(void);

/**
 * @brief Start/resume the sweep animation
 *
 * Resumes the sweep line animation if it was stopped.
 */
void radar_sweep_start_animation(void);

/**
 * @brief Clean up and delete all UI elements
 *
 * Stops animation and removes all objects.
 * Call this before switching to another screen.
 */
void radar_sweep_delete_ui(void);

#ifdef __cplusplus
}
#endif
