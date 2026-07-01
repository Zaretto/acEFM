#include "stdafx.h"
#include "gauge.h"
#include <imgui.h>

#define _USE_MATH_DEFINES
#include <cmath>
#include <cstdio>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace testbed {

static const ImU32 COL_BG    = IM_COL32(10,  10,  10,  255);
static const ImU32 COL_GREEN = IM_COL32(0,   255, 0,   255);
static const ImU32 COL_AMBER = IM_COL32(255, 176, 0,   255);
static const ImU32 COL_RED   = IM_COL32(255, 0,   0,   255);
static const ImU32 COL_WHITE = IM_COL32(255, 255, 255, 255);
static const ImU32 COL_DIM   = IM_COL32(60,  60,  60,  255);

// 270-degree sweep: 225 deg (bottom-left) clockwise to -45 deg (bottom-right)
static float value_to_angle(float value, float min_val, float max_val)
{
    float frac = (value - min_val) / (max_val - min_val);
    if (frac < 0.0f) frac = 0.0f;
    if (frac > 1.0f) frac = 1.0f;
    float start_angle = 225.0f * (float)M_PI / 180.0f;
    float sweep       = 270.0f * (float)M_PI / 180.0f;
    return start_angle - frac * sweep;
}

void draw_arc_gauge(ImDrawList* dl, ImVec2 center, float radius,
                    float value, float min_val, float max_val,
                    float caution_val, float warning_val,
                    const char* label, const char* fmt)
{
    const int segments = 64;
    float a_start = value_to_angle(min_val, min_val, max_val);
    float a_end   = value_to_angle(max_val, min_val, max_val);

    // Background arc
    for (int i = 0; i < segments; ++i) {
        float t0 = (float)i       / segments;
        float t1 = (float)(i + 1) / segments;
        float ang0 = a_start + t0 * (a_end - a_start);
        float ang1 = a_start + t1 * (a_end - a_start);
        float val0 = min_val + t0 * (max_val - min_val);
        ImU32 col  = (val0 >= warning_val) ? COL_RED :
                     (val0 >= caution_val) ? COL_AMBER : COL_DIM;
        ImVec2 p0(center.x + cosf(ang0) * radius, center.y - sinf(ang0) * radius);
        ImVec2 p1(center.x + cosf(ang1) * radius, center.y - sinf(ang1) * radius);
        dl->AddLine(p0, p1, col, 3.0f);
    }

    // Active arc
    float clamped  = value < min_val ? min_val : (value > max_val ? max_val : value);
    int active_seg = (int)((clamped - min_val) / (max_val - min_val) * segments);
    for (int i = 0; i < active_seg; ++i) {
        float t0 = (float)i       / segments;
        float t1 = (float)(i + 1) / segments;
        float ang0 = a_start + t0 * (a_end - a_start);
        float ang1 = a_start + t1 * (a_end - a_start);
        float val0 = min_val + t0 * (max_val - min_val);
        ImU32 col  = (val0 >= warning_val) ? COL_RED :
                     (val0 >= caution_val) ? COL_AMBER : COL_GREEN;
        ImVec2 p0(center.x + cosf(ang0) * radius, center.y - sinf(ang0) * radius);
        ImVec2 p1(center.x + cosf(ang1) * radius, center.y - sinf(ang1) * radius);
        dl->AddLine(p0, p1, col, 5.0f);
    }

    // Needle
    float a_val = value_to_angle(value, min_val, max_val);
    float nr    = radius * 0.75f;
    ImVec2 tip(center.x + cosf(a_val) * nr, center.y - sinf(a_val) * nr);
    dl->AddLine(center, tip, COL_WHITE, 2.0f);
    dl->AddCircleFilled(center, 4.0f, COL_WHITE);

    // Digital readout
    char buf[32];
    snprintf(buf, sizeof(buf), fmt, value);
    ImVec2 ts  = ImGui::CalcTextSize(buf);
    ImVec2 tp(center.x - ts.x * 0.5f, center.y + radius * 0.3f);
    dl->AddText(tp, COL_WHITE, buf);

    // Label
    ImVec2 ls = ImGui::CalcTextSize(label);
    ImVec2 lp(center.x - ls.x * 0.5f, center.y + radius * 0.55f);
    dl->AddText(lp, COL_GREEN, label);
}

void draw_tape_gauge(ImDrawList* dl, ImVec2 top_left, float width, float height,
                     float value, float min_val, float max_val,
                     const char* label, const char* fmt)
{
    float frac = (value - min_val) / (max_val - min_val);
    if (frac < 0.0f) frac = 0.0f;
    if (frac > 1.0f) frac = 1.0f;

    ImVec2 br(top_left.x + width, top_left.y + height);
    dl->AddRectFilled(top_left, br, IM_COL32(20, 20, 20, 255));
    dl->AddRect(top_left, br, COL_DIM);

    float   fill_h  = frac * height;
    ImVec2  fill_tl(top_left.x + 2, top_left.y + height - fill_h);
    ImVec2  fill_br(top_left.x + width - 2, top_left.y + height - 2);
    dl->AddRectFilled(fill_tl, fill_br, COL_GREEN);

    char buf[32];
    snprintf(buf, sizeof(buf), fmt, value);
    ImVec2 ts = ImGui::CalcTextSize(buf);
    ImVec2 tp(top_left.x + (width - ts.x) * 0.5f, top_left.y - ts.y - 2);
    dl->AddText(tp, COL_WHITE, buf);

    ImVec2 ls = ImGui::CalcTextSize(label);
    ImVec2 lp(top_left.x + (width - ls.x) * 0.5f, br.y + 2);
    dl->AddText(lp, COL_GREEN, label);
}

// ---- EADI helpers ----

// Clip line segment [pa,pb] to the interior of a circle.
// Returns false if the segment misses the circle entirely.
static bool clip_line_to_circle(ImVec2 ctr, float r,
                                ImVec2 pa, ImVec2 pb,
                                ImVec2& out_a, ImVec2& out_b)
{
    float dx = pb.x - pa.x, dy = pb.y - pa.y;
    float ex = pa.x - ctr.x, ey = pa.y - ctr.y;
    float A  = dx*dx + dy*dy;
    if (A < 1e-6f) return false;
    float B  = 2.0f*(dx*ex + dy*ey);
    float C  = ex*ex + ey*ey - r*r;
    float dsc = B*B - 4.0f*A*C;
    if (dsc < 0.0f) return false;
    float sq = sqrtf(dsc);
    float t1 = (-B - sq) / (2.0f*A);
    float t2 = (-B + sq) / (2.0f*A);
    if (t1 < 0.0f) t1 = 0.0f;
    if (t2 > 1.0f) t2 = 1.0f;
    if (t1 > t2) return false;
    out_a = ImVec2(pa.x + t1*dx, pa.y + t1*dy);
    out_b = ImVec2(pa.x + t2*dx, pa.y + t2*dy);
    return true;
}

void draw_eadi(ImDrawList* dl, ImVec2 ctr, float r,
               float pitch_deg, float roll_deg, float /*hdg_deg*/)
{
    const float PI = 3.14159265f;

    const ImU32 sky_col = IM_COL32( 38, 112, 196, 255);
    const ImU32 gnd_col = IM_COL32(105,  58,  10, 255);
    const ImU32 wht_col = IM_COL32(255, 255, 255, 255);
    const ImU32 ldr_sky = IM_COL32(255, 255, 255, 220);
    const ImU32 ldr_gnd = IM_COL32(220, 190, 110, 220);
    const ImU32 yel_col = IM_COL32(255, 210,   0, 255);
    const ImU32 arc_col = IM_COL32(200, 200, 200, 190);

    const float roll_rad = roll_deg * (PI / 180.0f);
    const float ppd      = r / 28.0f;  // pixels per degree

    // Horizon direction (unit vector along horizon line in screen)
    const float hdx = cosf(roll_rad);
    const float hdy = sinf(roll_rad);
    // Down direction in screen (perpendicular to horizon, toward ground)
    const float gx  = sinf(roll_rad);
    const float gy  = cosf(roll_rad);

    // Point on horizon corresponding to pitch offset from centre.
    // Nose-up (pitch>0) pushes horizon downward in the gravity direction.
    const float hmx = ctr.x + pitch_deg * ppd * gx;
    const float hmy = ctr.y + pitch_deg * ppd * gy;

    // ---- Sky fill ----
    dl->AddCircleFilled(ctr, r, sky_col, 64);

    // ---- Ground polygon ----
    {
        float dxh = hmx - ctr.x, dyh = hmy - ctr.y;
        // Intersect horizon line with circle; A=1 (unit direction vector)
        float B    = dxh*hdx + dyh*hdy;
        float C    = dxh*dxh + dyh*dyh - r*r;
        float disc = B*B - C;

        if (disc >= 0.0f) {
            float sq = sqrtf(disc);
            ImVec2 p1(hmx + (-B - sq)*hdx, hmy + (-B - sq)*hdy);
            ImVec2 p2(hmx + (-B + sq)*hdx, hmy + (-B + sq)*hdy);
            float a1 = atan2f(p1.y - ctr.y, p1.x - ctr.x);
            float a2 = atan2f(p2.y - ctr.y, p2.x - ctr.x);

            // Check whether the CCW arc from a1 to a2 is the ground arc.
            float a2n = a2; while (a2n < a1) a2n += 2.0f*PI;
            float amid = (a1 + a2n) * 0.5f;
            ImVec2 mp(ctr.x + r*cosf(amid), ctr.y + r*sinf(amid));
            bool ccw_is_ground = ((mp.x - hmx)*gx + (mp.y - hmy)*gy) > 0.0f;

            const int N = 32;
            ImVec2 poly[N + 4];
            int np = 0;

            if (ccw_is_ground) {
                poly[np++] = p1;
                for (int i = 0; i <= N; ++i) {
                    float a = a1 + (a2n - a1)*(float)i/(float)N;
                    poly[np++] = ImVec2(ctr.x + r*cosf(a), ctr.y + r*sinf(a));
                }
                poly[np++] = p2;
            } else {
                // Ground arc goes from a2 the long way around to a1.
                float a1n = a1; while (a1n < a2) a1n += 2.0f*PI;
                poly[np++] = p2;
                for (int i = 0; i <= N; ++i) {
                    float a = a2 + (a1n - a2)*(float)i/(float)N;
                    poly[np++] = ImVec2(ctr.x + r*cosf(a), ctr.y + r*sinf(a));
                }
                poly[np++] = p1;
            }
            dl->AddConvexPolyFilled(poly, np, gnd_col);
        } else {
            // Horizon doesn't cross circle: all sky or all ground.
            if ((ctr.x - hmx)*gx + (ctr.y - hmy)*gy > 0.0f)
                dl->AddCircleFilled(ctr, r, gnd_col, 64);
        }
    }

    // ---- Circle border ----
    dl->AddCircle(ctr, r, IM_COL32(80, 80, 80, 255), 64, 2.0f);

    // ---- Horizon line ----
    {
        ImVec2 la(hmx - hdx*r*2.0f, hmy - hdy*r*2.0f);
        ImVec2 lb(hmx + hdx*r*2.0f, hmy + hdy*r*2.0f);
        ImVec2 ca, cb;
        if (clip_line_to_circle(ctr, r - 1.0f, la, lb, ca, cb))
            dl->AddLine(ca, cb, wht_col, 2.0f);
    }

    // ---- Pitch ladder ----
    // Major lines (10° multiples): split with centre gap + perpendicular end ticks.
    // Minor lines (5° steps): short solid bar, no gap.
    const float gap_h = r * 0.11f;  // half-gap each side of centre
    const float tk_len = r * 0.06f; // end-tick length

    for (int pd = -80; pd <= 80; pd += 5) {
        if (pd == 0) continue;
        bool major = (pd % 10 == 0);
        float hw   = major ? r * 0.30f : r * 0.13f;

        float diff = pitch_deg - (float)pd;
        float lx = ctr.x + diff*ppd*gx;
        float ly = ctr.y + diff*ppd*gy;

        float ddx = lx - ctr.x, ddy = ly - ctr.y;
        if (ddx*ddx + ddy*ddy > (r + hw)*(r + hw)) continue;

        ImU32 col = (pd > 0) ? ldr_sky : ldr_gnd;

        // End-tick direction: toward the horizon (0° line) from the line position.
        // Sky marks (pd>0) are above horizon → tick downward (+gravity).
        // Ground marks (pd<0) are below horizon → tick upward (-gravity).
        float tkx = (pd > 0) ?  gx : -gx;
        float tky = (pd > 0) ?  gy : -gy;

        ImVec2 ca, cb;
        if (major) {
            // Left half: outer → gap edge
            ImVec2 lo(lx - hdx*hw,    ly - hdy*hw);
            ImVec2 li(lx - hdx*gap_h, ly - hdy*gap_h);
            if (clip_line_to_circle(ctr, r - 2.0f, lo, li, ca, cb)) {
                dl->AddLine(ca, cb, col, 1.5f);
                // tick at outer (ca) end, pointing toward horizon
                dl->AddLine(ca, ImVec2(ca.x + tkx*tk_len, ca.y + tky*tk_len), col, 1.5f);
            }
            // Right half: gap edge → outer
            ImVec2 ri(lx + hdx*gap_h, ly + hdy*gap_h);
            ImVec2 ro(lx + hdx*hw,    ly + hdy*hw);
            if (clip_line_to_circle(ctr, r - 2.0f, ri, ro, ca, cb)) {
                dl->AddLine(ca, cb, col, 1.5f);
                // tick at outer (cb) end
                dl->AddLine(cb, ImVec2(cb.x + tkx*tk_len, cb.y + tky*tk_len), col, 1.5f);
            }
            // Labels: anchored at unclipped outer endpoints, offset sky-ward
            char buf[6]; snprintf(buf, sizeof(buf), "%d", abs(pd));
            float ox = -gx * 7.0f, oy = -gy * 7.0f;
            if (pd < 0) { ox = -ox; oy = -oy; }
            dl->AddText(ImVec2(lo.x - hdx*14.0f + ox, lo.y - hdy*14.0f + oy - 6.0f), col, buf);
            dl->AddText(ImVec2(ro.x + hdx*  3.0f + ox, ro.y + hdy*  3.0f + oy - 6.0f), col, buf);
        } else {
            // Minor line: single short solid bar
            ImVec2 la(lx - hdx*hw, ly - hdy*hw);
            ImVec2 lb(lx + hdx*hw, ly + hdy*hw);
            if (clip_line_to_circle(ctr, r - 2.0f, la, lb, ca, cb))
                dl->AddLine(ca, cb, col, 1.2f);
        }
    }

    // ---- Roll scale: fixed bezel tick marks ----
    {
        float ar = r - 5.0f;
        static const float rtick_deg[] = {-90,-60,-45,-30,-20,-10, 0, 10, 20, 30, 45, 60, 90};
        static const float rtick_len[] = {  9,  7,  5,  8,  5,  5, 9,  5,  5,  8,  5,  7,  9};
        for (int i = 0; i < 13; ++i) {
            float ta = -(PI/2.0f) + rtick_deg[i]*(PI/180.0f);
            float tl = rtick_len[i];
            dl->AddLine(
                ImVec2(ctr.x + (ar - tl)*cosf(ta), ctr.y + (ar - tl)*sinf(ta)),
                ImVec2(ctr.x +  ar       *cosf(ta), ctr.y +  ar       *sinf(ta)),
                arc_col, 1.5f);
        }
        // Moving bank pointer triangle (points inward, rotates with roll)
        float pa = -(PI/2.0f) + roll_rad;
        float cp = cosf(pa), sp = sinf(pa);
        ImVec2 tip(ctr.x + (ar - 10.0f)*cp, ctr.y + (ar - 10.0f)*sp);
        ImVec2 bl (ctr.x + r*cosf(pa - 0.085f), ctr.y + r*sinf(pa - 0.085f));
        ImVec2 br (ctr.x + r*cosf(pa + 0.085f), ctr.y + r*sinf(pa + 0.085f));
        dl->AddTriangleFilled(tip, bl, br, yel_col);
    }

    // ---- Fixed aircraft reference symbol ----
    {
        float aw = r * 0.30f;
        float iw = r * 0.10f;
        float th = r * 0.10f;
        dl->AddLine(ImVec2(ctr.x - aw, ctr.y), ImVec2(ctr.x - iw, ctr.y), yel_col, 3.0f);
        dl->AddLine(ImVec2(ctr.x + iw, ctr.y), ImVec2(ctr.x + aw, ctr.y), yel_col, 3.0f);
        dl->AddLine(ImVec2(ctr.x - iw, ctr.y), ImVec2(ctr.x - iw, ctr.y - th), yel_col, 3.0f);
        dl->AddLine(ImVec2(ctr.x + iw, ctr.y), ImVec2(ctr.x + iw, ctr.y - th), yel_col, 3.0f);
        dl->AddCircleFilled(ctr, r * 0.04f, yel_col, 8);
    }
}

// ---- Heading tape (whiskey compass) ----

void draw_heading_tape(ImDrawList* dl, ImVec2 tl, float w, float h, float hdg_deg)
{
    const ImU32 bg  = IM_COL32(15,  15,  15,  210);
    const ImU32 tkc = IM_COL32(180, 180, 180, 220);
    const ImU32 wht = IM_COL32(255, 255, 255, 255);
    const ImU32 yel = IM_COL32(255, 210,   0, 255);

    dl->AddRectFilled(tl, ImVec2(tl.x + w, tl.y + h), bg);
    dl->AddRect(tl, ImVec2(tl.x + w, tl.y + h), IM_COL32(70, 70, 70, 255));

    const float cx     = tl.x + w * 0.5f;
    const float range  = 60.0f;   // total visible degrees
    const float ppdeg  = w / range;

    // Find lowest 5-degree tick to start from
    float d0 = floorf((hdg_deg - range * 0.5f) / 5.0f) * 5.0f;
    float d1 = d0 + range + 5.0f;

    for (float d = d0; d <= d1; d += 5.0f) {
        // Shortest angular path to current heading
        float diff = d - hdg_deg;
        while (diff >  180.0f) diff -= 360.0f;
        while (diff < -180.0f) diff += 360.0f;
        float sx = cx + diff * ppdeg;
        if (sx < tl.x - 1.0f || sx > tl.x + w + 1.0f) continue;

        int norm = (int)(d) % 360; if (norm < 0) norm += 360;
        bool at30 = (norm % 30 == 0);
        bool at10 = (norm % 10 == 0);
        float tk = at30 ? h * 0.65f : (at10 ? h * 0.42f : h * 0.25f);

        dl->AddLine(ImVec2(sx, tl.y + h), ImVec2(sx, tl.y + h - tk), tkc, 1.0f);

        if (at30) {
            char buf[8] = {};
            const char* label = buf;
            if      (norm ==   0) label = "N";
            else if (norm ==  90) label = "E";
            else if (norm == 180) label = "S";
            else if (norm == 270) label = "W";
            else snprintf(buf, sizeof(buf), "%03d", norm);
            ImVec2 ts = ImGui::CalcTextSize(label);
            dl->AddText(ImVec2(sx - ts.x * 0.5f, tl.y + 2.0f), wht, label);
        }
    }

    // Current-heading index: filled triangle at bottom centre
    dl->AddTriangleFilled(
        ImVec2(cx,     tl.y + h),
        ImVec2(cx - 5, tl.y + h - 7),
        ImVec2(cx + 5, tl.y + h - 7),
        yel);
}

// ---- Altitude tape ----

void draw_alt_tape(ImDrawList* dl, ImVec2 tl, float w, float h, float alt_ft)
{
    const ImU32 bg  = IM_COL32(15,  15,  15,  210);
    const ImU32 tkc = IM_COL32(160, 160, 160, 200);
    const ImU32 wht = IM_COL32(255, 255, 255, 255);
    const ImU32 yel = IM_COL32(255, 210,   0, 255);

    dl->AddRectFilled(tl, ImVec2(tl.x + w, tl.y + h), bg);
    dl->AddRect(tl, ImVec2(tl.x + w, tl.y + h), IM_COL32(70, 70, 70, 255));

    const float cy      = tl.y + h * 0.5f;
    const float px_ft   = h / 2200.0f;   // ±1100 ft visible
    const float step    = 200.0f;         // tick every 200 ft
    const float lblstep = 1000.0f;        // label every 1000 ft

    float a0 = floorf((alt_ft - h * 0.5f / px_ft) / step) * step;
    float a1 = a0 + h / px_ft + step;

    for (float a = a0; a <= a1; a += step) {
        float sy = cy - (a - alt_ft) * px_ft;
        if (sy < tl.y || sy > tl.y + h) continue;

        int ia   = (int)roundf(a);
        bool lbl = (ia % (int)lblstep == 0);
        float tw = lbl ? w * 0.45f : w * 0.25f;
        dl->AddLine(ImVec2(tl.x, sy), ImVec2(tl.x + tw, sy), tkc, 1.0f);

        if (lbl && fabsf(sy - cy) > 11.0f) {
            char buf[12]; snprintf(buf, sizeof(buf), "%d", ia);
            ImVec2 ts = ImGui::CalcTextSize(buf);
            dl->AddText(ImVec2(tl.x + tw + 2.0f, sy - ts.y * 0.5f),
                        IM_COL32(160, 160, 160, 220), buf);
        }
    }

    // Index pointer
    dl->AddLine(ImVec2(tl.x, cy), ImVec2(tl.x + w * 0.35f, cy), yel, 2.0f);

    // Rolling counter box at centre
    char buf[12]; snprintf(buf, sizeof(buf), "%-6.0f", alt_ft);
    ImVec2 ts = ImGui::CalcTextSize(buf);
    float bh = ts.y + 4.0f;
    dl->AddRectFilled(ImVec2(tl.x, cy - bh * 0.5f), ImVec2(tl.x + w, cy + bh * 0.5f),
                      IM_COL32(0, 0, 0, 230));
    dl->AddRect(ImVec2(tl.x, cy - bh * 0.5f), ImVec2(tl.x + w, cy + bh * 0.5f),
                IM_COL32(180, 180, 180, 200));
    dl->AddText(ImVec2(tl.x + (w - ts.x) * 0.5f, cy - ts.y * 0.5f), wht, buf);
}

// ---- VSI bar ----

void draw_vsi_bar(ImDrawList* dl, ImVec2 tl, float w, float h, float vsi_fpm)
{
    const float max_fpm = 6000.0f;

    dl->AddRectFilled(tl, ImVec2(tl.x + w, tl.y + h), IM_COL32(15, 15, 15, 210));
    dl->AddRect(tl, ImVec2(tl.x + w, tl.y + h), IM_COL32(70, 70, 70, 255));

    const float cy      = tl.y + h * 0.5f;
    const float px_fpm  = h * 0.5f / max_fpm;

    // Tick marks at ±2000 and ±4000 fpm
    for (float t : {2000.0f, 4000.0f}) {
        float off = t * px_fpm;
        dl->AddLine(ImVec2(tl.x, cy - off), ImVec2(tl.x + w * 0.55f, cy - off),
                    IM_COL32(120, 120, 120, 180), 1.0f);
        dl->AddLine(ImVec2(tl.x, cy + off), ImVec2(tl.x + w * 0.55f, cy + off),
                    IM_COL32(120, 120, 120, 180), 1.0f);
    }

    // Filled bar (green=climb, amber=descent)
    float clamped = vsi_fpm < -max_fpm ? -max_fpm : (vsi_fpm > max_fpm ? max_fpm : vsi_fpm);
    float bar_px  = clamped * px_fpm;   // positive → up in screen (−y)
    if (fabsf(bar_px) > 0.5f) {
        float y1 = bar_px > 0.0f ? cy - bar_px : cy;
        float y2 = bar_px > 0.0f ? cy           : cy - bar_px;
        ImU32 col = clamped > 0.0f ? IM_COL32(0, 200, 80, 200) : IM_COL32(255, 140, 0, 200);
        dl->AddRectFilled(ImVec2(tl.x + w * 0.15f, y1), ImVec2(tl.x + w * 0.85f, y2), col);
    }

    // Zero line
    dl->AddLine(ImVec2(tl.x, cy), ImVec2(tl.x + w, cy),
                IM_COL32(200, 200, 200, 200), 1.5f);
}

} // namespace testbed
