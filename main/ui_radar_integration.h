/*
 * ui_radar_integration.h
 * Helper functions for integrating and switching between radar displays
 */

#pragma once

#include "lvgl.h"
#include "humanRadarRD_03D.h"

#ifdef __cplusplus
extern "C" {
#endif

// Display modes
typedef enum {
    DISPLAY_MODE_LIST,      // List view (ui_radar_display)
    DISPLAY_MODE_SWEEP,     // Radar sweep view (ui_radar_sweep)
} display_mode_t;

/**
 * @brief Initialize the radar display system
 *
 * Sets up the initial display mode. Call this once during startup
 * after LVGL has been initialized.
 *
 * @param disp LVGL display handle
 * @param initial_mode Initial display mode to use
 */
void radar_display_init(lv_display_t *disp, display_mode_t initial_mode);

/**
 * @brief Switch between display modes
 *
 * Toggles between LIST and SWEEP display modes.
 * Call this from a button handler or menu action.
 *
 * @param disp LVGL display handle
 */
void radar_switch_display_mode(lv_display_t *disp);

/**
 * @brief Update the currently active display
 *
 * Routes the update to the appropriate display function based
 * on the current display mode. Handles display locking internally.
 *
 * @param targets Array of radar_target_t structures
 * @param targetId Target ID to update (0-2)
 * @param hasMoved Boolean indicating if the target has moved
 */
void radar_update_current_display(radar_target_t *targets, int targetId, bool hasMoved);

/**
 * @brief Get the current display mode
 *
 * @return Current display_mode_t value
 */
display_mode_t radar_get_display_mode(void);

/**
 * @brief Example button handler with display switching
 *
 * This is a reference implementation showing how to integrate
 * display switching with button handlers.
 *
 * @param button_handle Button handle
 * @param usr_data User data (button index)
 * @param disp LVGL display handle
 */
void radar_btn_handler_example(void *button_handle, void *usr_data, lv_display_t *disp);

#ifdef __cplusplus
}
#endif
