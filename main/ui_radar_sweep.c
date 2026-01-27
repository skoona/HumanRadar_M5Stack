/*
 * ui_radar_sweep.c
 * LVGL radar sweep display for radar_target_t structure
 * Screen: 320x240 pixels
 * Radar: ±60° sweep, 8 meters range
 * ESP-IDF v5.5.2, LVGL v9.4
 */

#include "lvgl.h"
#include "humanRadarRD_03D.h"
#include "esp_log.h"
#include <math.h>

#define LV_SYMBOL_USER "\xEF\x81\xB0"  // Custom user symbol

#ifndef PI
#define PI (3.14159265358979f)
#endif

#define RADAR_CENTER_X 160  // Screen center X
#define RADAR_CENTER_Y 220  // Radar at bottom of screen
#define RADAR_MAX_RANGE 8000.0f  // 8 meters in mm
#define RADAR_RADIUS 180  // Display radius in pixels
#define RADAR_SWEEP_ANGLE 60  // ±60 degrees = 120 total
#define SWEEP_SPEED 3  // Degrees per timer tick

static const char *TAG = "RadarSweep";

// UI elements
typedef struct {
    lv_obj_t *scr;
    lv_obj_t *radar_base;  // Container for radar graphics
    lv_obj_t *sweep_line;  // The animated sweep line
    lv_obj_t *arc_lines[61];  // Lines forming the outer arc
    lv_obj_t *range_rings[4][61];  // Range ring lines
    lv_obj_t *angle_lines[3];  // Center angle markers
    lv_obj_t *range_labels[4];  // Range text labels
    lv_obj_t *shadow_lines[30];  // Shadow trail lines
    lv_obj_t *target_markers[RADAR_MAX_TARGETS];
    lv_obj_t *target_labels[RADAR_MAX_TARGETS];
    lv_obj_t *info_label;
    int16_t current_angle;  // Current sweep angle (-60 to +60)
    int8_t sweep_direction;  // 1 = right, -1 = left
} radar_sweep_ui_t;

static radar_sweep_ui_t ui;
static lv_timer_t *sweep_timer = NULL;

/**
 * @brief Convert polar coordinates to screen cartesian coordinates
 */
static void polar_to_screen(float distance_mm, float angle_deg, int16_t *x, int16_t *y)
{
    // Normalize distance to display radius
    float normalized_distance = (distance_mm / RADAR_MAX_RANGE) * RADAR_RADIUS;
    if (normalized_distance > RADAR_RADIUS) {
        normalized_distance = RADAR_RADIUS;
    }

    // Convert angle to radians (0° = up, positive = clockwise)
    float angle_rad = (angle_deg) * PI / 180.0f;

    // Calculate screen coordinates (Y-axis inverted for screen)
    *x = RADAR_CENTER_X + (int16_t)(normalized_distance * sinf(angle_rad));
    *y = RADAR_CENTER_Y - (int16_t)(normalized_distance * cosf(angle_rad));
}

/**
 * @brief Create a line object between two points
 */
static lv_obj_t *create_line(lv_obj_t *parent, int16_t x1, int16_t y1, int16_t x2, int16_t y2,
                             lv_color_t color, int16_t width, lv_opa_t opa)
{
    static lv_point_precise_t points[2];
    points[0].x = x1;
    points[0].y = y1;
    points[1].x = x2;
    points[1].y = y2;

    lv_obj_t *line = lv_line_create(parent);
    lv_line_set_points(line, points, 2);
    lv_obj_set_style_line_color(line, color, 0);
    lv_obj_set_style_line_width(line, width, 0);
    lv_obj_set_style_line_opa(line, opa, 0);

    return line;
}

/**
 * @brief Draw radar background (arc, range rings, angle markers)
 */
static void create_radar_background(lv_obj_t *parent)
{
    // Create outer arc with line segments
    for (int i = 0; i < 61; i++) {
        int angle = -60 + i * 2;
        float angle1_rad = angle * PI / 180.0f;
        float angle2_rad = (angle + 2) * PI / 180.0f;

        int16_t x1 = RADAR_CENTER_X + (int16_t)(RADAR_RADIUS * sinf(angle1_rad));
        int16_t y1 = RADAR_CENTER_Y - (int16_t)(RADAR_RADIUS * cosf(angle1_rad));
        int16_t x2 = RADAR_CENTER_X + (int16_t)(RADAR_RADIUS * sinf(angle2_rad));
        int16_t y2 = RADAR_CENTER_Y - (int16_t)(RADAR_RADIUS * cosf(angle2_rad));

        ui.arc_lines[i] = create_line(parent, x1, y1, x2, y2, lv_color_hex(0x00FF00), 2, LV_OPA_COVER);
    }

    // Create range rings (2m, 4m, 6m, 8m)
    for (int ring = 0; ring < 4; ring++) {
        float ring_radius = (RADAR_RADIUS * (ring + 1)) / 4.0f;

        for (int i = 0; i < 61; i++) {
            int angle = -60 + i * 2;
            float angle1_rad = angle * PI / 180.0f;
            float angle2_rad = (angle + 2) * PI / 180.0f;

            int16_t x1 = RADAR_CENTER_X + (int16_t)(ring_radius * sinf(angle1_rad));
            int16_t y1 = RADAR_CENTER_Y - (int16_t)(ring_radius * cosf(angle1_rad));
            int16_t x2 = RADAR_CENTER_X + (int16_t)(ring_radius * sinf(angle2_rad));
            int16_t y2 = RADAR_CENTER_Y - (int16_t)(ring_radius * cosf(angle2_rad));

            ui.range_rings[ring][i] = create_line(parent, x1, y1, x2, y2,
                                                   lv_color_hex(0x003300), 1, LV_OPA_COVER);
        }
    }

    // Create center angle lines (0°, -60°, +60°)
    // Center line (0°)
    ui.angle_lines[0] = create_line(parent, RADAR_CENTER_X, RADAR_CENTER_Y,
                                     RADAR_CENTER_X, RADAR_CENTER_Y - RADAR_RADIUS,
                                     lv_color_hex(0x004400), 1, LV_OPA_COVER);

    // Left line (-60°)
    int16_t left_x = RADAR_CENTER_X - (int16_t)(RADAR_RADIUS * sinf(60.0f * PI / 180.0f));
    int16_t left_y = RADAR_CENTER_Y - (int16_t)(RADAR_RADIUS * cosf(60.0f * PI / 180.0f));
    ui.angle_lines[1] = create_line(parent, RADAR_CENTER_X, RADAR_CENTER_Y,
                                     left_x, left_y,
                                     lv_color_hex(0x004400), 1, LV_OPA_COVER);

    // Right line (+60°)
    int16_t right_x = RADAR_CENTER_X + (int16_t)(RADAR_RADIUS * sinf(60.0f * PI / 180.0f));
    int16_t right_y = RADAR_CENTER_Y - (int16_t)(RADAR_RADIUS * cosf(60.0f * PI / 180.0f));
    ui.angle_lines[2] = create_line(parent, RADAR_CENTER_X, RADAR_CENTER_Y,
                                     right_x, right_y,
                                     lv_color_hex(0x004400), 1, LV_OPA_COVER);

    // Create range labels
    for (int i = 0; i < 4; i++) {
        ui.range_labels[i] = lv_label_create(parent);
        lv_label_set_text_fmt(ui.range_labels[i], "%dm", (i + 1) * 2);
        lv_obj_set_style_text_color(ui.range_labels[i], lv_color_hex(0x00AA00), 0);
        lv_obj_set_style_text_font(ui.range_labels[i], &lv_font_montserrat_10, 0);
        int16_t label_y = RADAR_CENTER_Y - (RADAR_RADIUS * (i + 1)) / 4 - 5;
        lv_obj_set_pos(ui.range_labels[i], RADAR_CENTER_X + 5, label_y);
    }

    // Create shadow lines (initially hidden)
    for (int i = 0; i < 30; i++) {
        ui.shadow_lines[i] = create_line(parent, RADAR_CENTER_X, RADAR_CENTER_Y,
                                         RADAR_CENTER_X, RADAR_CENTER_Y - RADAR_RADIUS,
                                         lv_color_hex(0x00FF00), 1, LV_OPA_TRANSP);
    }
}

/**
 * @brief Update sweep line position
 */
static void update_sweep_line(int16_t angle)
{
    // Update shadow trail
    for (int i = 0; i < 30; i++) {
        int16_t shadow_angle = angle - (i * ui.sweep_direction);

        // Calculate opacity (brighter near sweep line)
        lv_opa_t opacity = (lv_opa_t)(255 - (i * 255 / 30)) / 4;

        // Calculate end point
        float angle_rad = shadow_angle * PI / 180.0f;
        int16_t end_x = RADAR_CENTER_X + (int16_t)(RADAR_RADIUS * sinf(angle_rad));
        int16_t end_y = RADAR_CENTER_Y - (int16_t)(RADAR_RADIUS * cosf(angle_rad));

        static lv_point_precise_t points[2];
        points[0].x = RADAR_CENTER_X;
        points[0].y = RADAR_CENTER_Y;
        points[1].x = end_x;
        points[1].y = end_y;

        lv_line_set_points(ui.shadow_lines[i], points, 2);
        lv_obj_set_style_line_opa(ui.shadow_lines[i], opacity, 0);
    }

    // Update main sweep line
    float angle_rad = angle * PI / 180.0f;
    int16_t end_x = RADAR_CENTER_X + (int16_t)(RADAR_RADIUS * sinf(angle_rad));
    int16_t end_y = RADAR_CENTER_Y - (int16_t)(RADAR_RADIUS * cosf(angle_rad));

    static lv_point_precise_t points[2];
    points[0].x = RADAR_CENTER_X;
    points[0].y = RADAR_CENTER_Y;
    points[1].x = end_x;
    points[1].y = end_y;

    lv_line_set_points(ui.sweep_line, points, 2);
}

/**
 * @brief Timer callback for sweep animation
 */
static void sweep_timer_cb(lv_timer_t *timer)
{
    // Update sweep angle
    ui.current_angle += SWEEP_SPEED * ui.sweep_direction;

    // Reverse direction at limits
    if (ui.current_angle >= RADAR_SWEEP_ANGLE) {
        ui.current_angle = RADAR_SWEEP_ANGLE;
        ui.sweep_direction = -1;
    } else if (ui.current_angle <= -RADAR_SWEEP_ANGLE) {
        ui.current_angle = -RADAR_SWEEP_ANGLE;
        ui.sweep_direction = 1;
    }

    // Update sweep line and shadow
    update_sweep_line(ui.current_angle);
}

/**
 * @brief Create the radar sweep UI
 */
void radar_sweep_create_ui(lv_obj_t *parent)
{
    ui.scr = parent;
    ui.current_angle = -RADAR_SWEEP_ANGLE;
    ui.sweep_direction = 1;

	// Set background color to black
    lv_obj_set_style_bg_color(parent, lv_color_hex(0x000000), 0);

    // Create radar base container
    ui.radar_base = lv_obj_create(parent);
    lv_obj_set_size(ui.radar_base, 320, 240);
    lv_obj_set_style_bg_opa(ui.radar_base, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(ui.radar_base, 0, 0);
    lv_obj_set_style_pad_all(ui.radar_base, 0, 0);
    lv_obj_center(ui.radar_base);

    // Draw radar background
    create_radar_background(ui.radar_base);

    // Create main sweep line
    ui.sweep_line = create_line(ui.radar_base, RADAR_CENTER_X, RADAR_CENTER_Y,
                                RADAR_CENTER_X, RADAR_CENTER_Y - RADAR_RADIUS,
                                lv_color_hex(0x00FF00), 2, LV_OPA_COVER);

    // Create target markers (hidden initially)
    for (int i = 0; i < RADAR_MAX_TARGETS; i++) {
        ui.target_markers[i] = lv_label_create(parent);
        lv_label_set_text(ui.target_markers[i], LV_SYMBOL_USER);
        lv_obj_set_style_text_color(ui.target_markers[i], lv_color_hex(0xFF0000), 0);
        lv_obj_set_style_text_font(ui.target_markers[i], &lv_font_montserrat_20, 0);
        lv_obj_add_flag(ui.target_markers[i], LV_OBJ_FLAG_HIDDEN);

        // Create small label for target ID
        ui.target_labels[i] = lv_label_create(parent);
        lv_label_set_text_fmt(ui.target_labels[i], "T%d", i);
        lv_obj_set_style_text_color(ui.target_labels[i], lv_color_hex(0xFFFF00), 0);
        lv_obj_set_style_text_font(ui.target_labels[i], &lv_font_montserrat_10, 0);
        lv_obj_add_flag(ui.target_labels[i], LV_OBJ_FLAG_HIDDEN);
    }

    // Create info label at top
    ui.info_label = lv_label_create(parent);
    lv_label_set_text(ui.info_label, "Radar: 8m / ±60°");
    lv_obj_set_style_text_color(ui.info_label, lv_color_hex(0x00FF00), 0);
    lv_obj_set_style_text_font(ui.info_label, &lv_font_montserrat_12, 0);
    lv_obj_align(ui.info_label, LV_ALIGN_TOP_MID, 0, 5);

    // Start sweep animation timer
    sweep_timer = lv_timer_create(sweep_timer_cb, 50, NULL);  // 50ms = 20 FPS
}

/**
 * @brief Update target positions on radar
 */
void radar_sweep_update(radar_target_t *targets, int targetId, bool hasMoved)
{
    if (targets == NULL || targetId < 0 || targetId >= RADAR_MAX_TARGETS) {
        return;
    }

    if (targets[targetId].detected && hasMoved) {
        // Calculate screen position from polar coordinates
        int16_t screen_x, screen_y;
        polar_to_screen(targets[targetId].distance,
                       targets[targetId].angle,
                       &screen_x, &screen_y);

        // Show and position target marker
        lv_obj_clear_flag(ui.target_markers[targetId], LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_pos(ui.target_markers[targetId],
                      screen_x - 10,  // Center the symbol
                      screen_y - 10);

        // Show and position target label
        lv_obj_clear_flag(ui.target_labels[targetId], LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_pos(ui.target_labels[targetId],
                      screen_x - 5,   // Offset slightly
                      screen_y + 12);

        // Change color based on distance (closer = more red)
        float distance_ratio = targets[targetId].distance / RADAR_MAX_RANGE;
        if (distance_ratio < 0.3f) {
            lv_obj_set_style_text_color(ui.target_markers[targetId],
                                       lv_color_hex(0xFF0000), 0);  // Red - close
        } else if (distance_ratio < 0.6f) {
            lv_obj_set_style_text_color(ui.target_markers[targetId],
                                       lv_color_hex(0xFFAA00), 0);  // Orange - medium
        } else {
            lv_obj_set_style_text_color(ui.target_markers[targetId],
                                       lv_color_hex(0x00FF00), 0);  // Green - far
        }

        ESP_LOGI(TAG, "Target %d at screen pos (%d, %d), distance %.0fmm, angle %.1f°",
                targetId, screen_x, screen_y,
                targets[targetId].distance, targets[targetId].angle);

    } else if (!targets[targetId].detected) {
        // Hide target marker
        lv_obj_add_flag(ui.target_markers[targetId], LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui.target_labels[targetId], LV_OBJ_FLAG_HIDDEN);
    }
}

/**
 * @brief Update info label with target count
 */
void radar_sweep_update_info(int target_count)
{
    lv_label_set_text_fmt(ui.info_label, "Radar: 8m / ±60° | Targets: %d", target_count);
}

/**
 * @brief Stop sweep animation
 */
void radar_sweep_stop_animation(void)
{
    if (sweep_timer) {
        lv_timer_del(sweep_timer);
        sweep_timer = NULL;
    }
}

/**
 * @brief Resume sweep animation
 */
void radar_sweep_start_animation(void)
{
    if (!sweep_timer) {
        sweep_timer = lv_timer_create(sweep_timer_cb, 50, NULL);
    }
}

/**
 * @brief Clean up and delete UI
 */
void radar_sweep_delete_ui(void)
{
    radar_sweep_stop_animation();

    if (ui.radar_base) {
        lv_obj_del(ui.radar_base);
        ui.radar_base = NULL;
    }

    for (int i = 0; i < RADAR_MAX_TARGETS; i++) {
        if (ui.target_markers[i]) {
            lv_obj_del(ui.target_markers[i]);
            ui.target_markers[i] = NULL;
        }
        if (ui.target_labels[i]) {
            lv_obj_del(ui.target_labels[i]);
            ui.target_labels[i] = NULL;
        }
    }

    if (ui.info_label) {
        lv_obj_del(ui.info_label);
        ui.info_label = NULL;
    }
}
