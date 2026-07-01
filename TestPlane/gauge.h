#pragma once

struct ImVec2;
struct ImDrawList;

namespace testbed {

void draw_arc_gauge(ImDrawList* dl, ImVec2 center, float radius,
                    float value, float min_val, float max_val,
                    float caution_val, float warning_val,
                    const char* label, const char* fmt = "%.1f");

void draw_tape_gauge(ImDrawList* dl, ImVec2 top_left, float width, float height,
                     float value, float min_val, float max_val,
                     const char* label, const char* fmt = "%.0f");

// Electronic Attitude Director Indicator.
// pitch_deg > 0 = nose up; roll_deg > 0 = right bank; hdg_deg = true heading 0-360.
void draw_eadi(ImDrawList* dl, ImVec2 center, float radius,
               float pitch_deg, float roll_deg, float hdg_deg);

// Whiskey-compass style heading tape. top_left / width / height define the bounding box.
void draw_heading_tape(ImDrawList* dl, ImVec2 top_left, float width, float height,
                       float hdg_deg);

// Vertical altitude tape; alt_ft is height above MSL.
void draw_alt_tape(ImDrawList* dl, ImVec2 top_left, float width, float height,
                   float alt_ft);

// Thin vertical VSI bar; vsi_fpm positive = climb, negative = descent.
void draw_vsi_bar(ImDrawList* dl, ImVec2 top_left, float width, float height,
                  float vsi_fpm);

} // namespace testbed
