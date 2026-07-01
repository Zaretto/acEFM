#include "stdafx.h"
#include "instruments.h"
#include "gauge.h"

#include <GL/gl.h>
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_opengl2.h>

#include <string>

// Both exported from acEFM.dll with C linkage; no shared header covers both.
extern "C" double ed_fm_get_param(unsigned index);
extern "C" void   ed_fm_set_command(int icommand, float value);
extern "C" double acefm_get_property(const char* path);
extern "C" int    acefm_get_command_count();
extern "C" void   acefm_get_command_info(int index, int* out_icommand, char* out_name, int name_max,
                                         int* out_discrete, float* out_value, float* out_min, float* out_max);
extern "C" double acefm_get_command_readback(int icommand);

// ---- Win32 + WGL state ----
static HWND  g_hwnd = nullptr;
static HDC   g_hdc  = nullptr;
static HGLRC g_hrc  = nullptr;
static int   g_w    = 1600;
static int   g_h    = 820;

// ---- WndProc ----
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

static LRESULT WINAPI InstrWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;
    switch (msg) {
    case WM_SIZE:
        if (wParam != SIZE_MINIMIZED) {
            g_w = (int)LOWORD(lParam);
            g_h = (int)HIWORD(lParam);
        }
        return 0;
    case WM_CLOSE:
        ShowWindow(hWnd, SW_HIDE);
        return 0;
    case WM_DESTROY:
        g_hwnd = nullptr;
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

bool InstrumentsInit(HINSTANCE hInstance)
{
    WNDCLASSEXW wc   = {};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_OWNDC;
    wc.lpfnWndProc   = InstrWndProc;
    wc.hInstance     = hInstance;
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"acEFM_Instr";
    RegisterClassExW(&wc);

    g_hwnd = CreateWindowExW(0, L"acEFM_Instr", L"acEFM Instruments",
        WS_OVERLAPPEDWINDOW, 80, 80, g_w, g_h, nullptr, nullptr, hInstance, nullptr);
    if (!g_hwnd) return false;

    g_hdc = GetDC(g_hwnd);

    PIXELFORMATDESCRIPTOR pfd = {};
    pfd.nSize        = sizeof(pfd);
    pfd.nVersion     = 1;
    pfd.dwFlags      = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType   = PFD_TYPE_RGBA;
    pfd.cColorBits   = 32;
    pfd.cDepthBits   = 24;
    pfd.cStencilBits = 8;
    pfd.iLayerType   = PFD_MAIN_PLANE;
    int pf = ChoosePixelFormat(g_hdc, &pfd);
    if (!pf || !SetPixelFormat(g_hdc, pf, &pfd)) {
        ReleaseDC(g_hwnd, g_hdc);
        DestroyWindow(g_hwnd);
        g_hwnd = nullptr;
        return false;
    }
    g_hrc = wglCreateContext(g_hdc);
    wglMakeCurrent(g_hdc, g_hrc);

    ShowWindow(g_hwnd, SW_SHOW);
    UpdateWindow(g_hwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io    = ImGui::GetIO();
    io.IniFilename = "acefm_instruments.ini";
    ImGui::StyleColorsDark();
    ImGui::GetStyle().WindowRounding = 2.0f;

    ImGui_ImplWin32_Init(g_hwnd);
    ImGui_ImplOpenGL2_Init();
    return true;
}

// ---- helpers ----

static double gprop(const char* path)        { return acefm_get_property(path); }
static double gprop(const std::string& s)    { return acefm_get_property(s.c_str()); }

// ---- engine panel ----

static void engine_panel()
{
    static const float R     = 62.0f;
    static const float GAPX  = R + 14.0f;
    static const float GAPY  = R + 10.0f;
    static const float DUMMY = R * 2.0f + 32.0f;

    ImGui::SetNextWindowPos(ImVec2(324, 4), ImGuiCond_Once);
    ImGui::SetNextWindowSize(ImVec2(510, 510), ImGuiCond_Once);
    ImGui::Begin("Engines");

    ImDrawList* dl = ImGui::GetWindowDrawList();

    // ---- APU (col 0) ----
    {
        ImGui::BeginGroup();
        ImGui::Text("--- APU ---");

        float rpm    = (float)gprop("/fdm/jsbsim/systems/apu/rpm-display");
        float egt    = (float)gprop("/fdm/jsbsim/systems/apu/egt-display-c");
        bool  online = gprop("/fdm/jsbsim/systems/apu/online")           > 0.5;
        bool  gen    = gprop("/fdm/jsbsim/systems/apu/generator-online") > 0.5;
        bool  fault  = gprop("/fdm/jsbsim/systems/apu/fault")            > 0.5;

        // RPM arc
        {
            ImVec2 c = ImGui::GetCursorScreenPos();
            testbed::draw_arc_gauge(dl, ImVec2(c.x + GAPX, c.y + GAPY), R,
                rpm, 0.0f, 100.0f, 95.0f, 100.0f, "RPM %", "%.0f");
            ImGui::Dummy(ImVec2(R * 2.0f + 28.0f, DUMMY));
        }
        // EGT arc
        {
            ImVec2 c = ImGui::GetCursorScreenPos();
            testbed::draw_arc_gauge(dl, ImVec2(c.x + GAPX, c.y + GAPY), R,
                egt, 0.0f, 750.0f, 620.0f, 650.0f, "EGT\xB0""C", "%.0f");
            ImGui::Dummy(ImVec2(R * 2.0f + 28.0f, DUMMY));
        }

        ImGui::Dummy(ImVec2(80, 90)); // nozzle placeholder

        ImVec4 dim(0.4f, 0.4f, 0.4f, 1.0f);
        ImVec4 grn(0.1f, 1.0f, 0.2f, 1.0f);
        ImVec4 red(1.0f, 0.2f, 0.1f, 1.0f);
        ImGui::TextColored(online ? grn : dim, "ONLINE  %d", (int)online);
        ImGui::TextColored(gen    ? grn : dim, "GEN     %d", (int)gen);
        ImGui::TextColored(fault  ? red : dim, "FAULT   %d", (int)fault);

        ImGui::EndGroup();
    }

    // ---- Main engines (col 1, 2) ----
    for (int e = 0; e < 2; ++e) {
        ImGui::SameLine((float)(e + 1) * 165.0f);
        ImGui::BeginGroup();

        std::string pb = "/fdm/jsbsim/propulsion/engine[" + std::to_string(e) + "]/";
        float n1     = (float)gprop(pb + "n1");
        float n2     = (float)gprop(pb + "n2");
        float egt    = (float)gprop(pb + "temp-display-C");
        float nozl   = (float)gprop(pb + "alt/nozzle-pos-norm");
        float ff_pph = (float)gprop(pb + "fuel-flow-rate-pps") * 3600.0f;
        float oil    = (float)gprop(pb + "oil-pressure-psi");
        float thrust = (float)gprop(pb + "thrust-lbs");

        ImGui::Text("--- ENG %d ---", e + 1);

        {
            ImVec2 c = ImGui::GetCursorScreenPos();
            testbed::draw_arc_gauge(dl, ImVec2(c.x + GAPX, c.y + GAPY), R,
                n2, 0.0f, 120.0f, 95.0f, 106.0f, "N2 %", "%.1f");
            ImGui::Dummy(ImVec2(R * 2.0f + 28.0f, DUMMY));
        }
        {
            ImVec2 c = ImGui::GetCursorScreenPos();
            testbed::draw_arc_gauge(dl, ImVec2(c.x + GAPX, c.y + GAPY), R,
                egt, 0.0f, 1200.0f, 900.0f, 1050.0f, "EGT\xB0""C", "%.0f");
            ImGui::Dummy(ImVec2(R * 2.0f + 28.0f, DUMMY));
        }
        {
            ImVec2 c = ImGui::GetCursorScreenPos();
            testbed::draw_tape_gauge(dl, ImVec2(c.x + 10, c.y + 14), 56, 70,
                nozl, 0.0f, 1.0f, "NOZL", "%.2f");
            ImGui::Dummy(ImVec2(80, 90));
        }

        ImGui::Text("N1    %5.1f %%",    n1);
        ImGui::Text("FF    %6.0f PPH",   ff_pph);
        ImGui::Text("OIL   %5.1f PSI",   oil);
        ImGui::Text("THRST %6.0f lbs",   thrust);

        ImGui::EndGroup();
    }
    ImGui::End();
}

// ---- EADI panel ----

static void eadi_panel()
{
    static const float R      = 110.0f;
    static const float HDG_H  =  22.0f;  // heading tape height
    static const float ALT_W  =  52.0f;  // altitude tape width
    static const float VSI_W  =  18.0f;  // VSI bar width
    static const float GAP    =   5.0f;
    static const float TEXT_H =  20.0f;

    // Total content dimensions (inside window padding)
    const float cw = R * 2.0f + GAP + ALT_W + GAP + VSI_W;
    const float ch = HDG_H + GAP + R * 2.0f + GAP + TEXT_H;

    ImGui::SetNextWindowPos(ImVec2(4, 4), ImGuiCond_Once);
    ImGui::SetNextWindowSize(ImVec2(cw + 16.0f, ch + 42.0f), ImGuiCond_Once);
    ImGui::Begin("EADI");

    static const double R2D = 180.0 / 3.14159265358979;
    float pitch = (float)(acefm_get_property("/fdm/jsbsim/attitude/theta-rad") * R2D);
    float roll  = (float)(acefm_get_property("/fdm/jsbsim/attitude/phi-rad")   * R2D);
    float hdg   = (float)(acefm_get_property("/fdm/jsbsim/attitude/psi-rad")   * R2D);
    float alt   = (float) acefm_get_property("/fdm/jsbsim/position/h-sl-ft");
    // vd-fps positive = descending; negate and convert to ft/min for VSI
    float vsi   = (float)(acefm_get_property("/fdm/jsbsim/velocities/vd-fps") * -60.0);
    float mach  = (float) acefm_get_property("/fdm/jsbsim/velocities/mach");
    float gload = (float) acefm_get_property("/fdm/jsbsim/accelerations/Nz");

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 org = ImGui::GetCursorScreenPos();
    ImGui::Dummy(ImVec2(cw, ch));

    float eadi_left = org.x;
    float eadi_top  = org.y + HDG_H + GAP;

    // Heading tape at top, spanning EADI diameter
    testbed::draw_heading_tape(dl, ImVec2(eadi_left, org.y), R * 2.0f, HDG_H, hdg);

    // EADI horizon ball
    testbed::draw_eadi(dl, ImVec2(eadi_left + R, eadi_top + R), R, pitch, roll, hdg);

    // Altitude tape to the right of EADI
    float alt_x = eadi_left + R * 2.0f + GAP;
    testbed::draw_alt_tape(dl, ImVec2(alt_x, eadi_top), ALT_W, R * 2.0f, alt);

    // VSI bar to the right of altitude tape
    float vsi_x = alt_x + ALT_W + GAP;
    testbed::draw_vsi_bar(dl, ImVec2(vsi_x, eadi_top), VSI_W, R * 2.0f, vsi);

    // Bottom row: Mach left, G-load right
    float text_y = eadi_top + R * 2.0f + GAP + 3.0f;
    char buf[24];
    snprintf(buf, sizeof(buf), "M%.3f", mach);
    dl->AddText(ImVec2(eadi_left, text_y), IM_COL32(0, 220, 220, 255), buf);
    snprintf(buf, sizeof(buf), "%.1fG", gload);
    ImVec2 gts = ImGui::CalcTextSize(buf);
    dl->AddText(ImVec2(eadi_left + R * 2.0f - gts.x, text_y), IM_COL32(0, 220, 220, 255), buf);

    ImGui::End();
}

// ---- param table ----

struct ParamEntry { unsigned idx; const char* name; };

static const ParamEntry k_params[] = {
    // APU: DCS engine 0 (base 0)
    {0,   "APU_RPM"},        {12,  "APU_TEMP"},      {15,  "APU_RUNNING"},
    // Main engines
    {100, "E1_RPM/N1"},      {101, "E1_REL_RPM"},    {102, "E1_CORE/N2"},
    {103, "E1_CREL_RPM"},    {104, "E1_THRUST_N"},   {112, "E1_TEMP"},
    {113, "E1_OIL"},         {114, "E1_FUELFLOW"},   {115, "E1_COMBUSTION"},
    {200, "E2_RPM/N1"},      {201, "E2_REL_RPM"},    {202, "E2_CORE/N2"},
    {203, "E2_CREL_RPM"},    {204, "E2_THRUST_N"},   {212, "E2_TEMP"},
    {213, "E2_OIL"},         {214, "E2_FUELFLOW"},   {215, "E2_COMBUSTION"},
    // Gear / suspension
    {2001, "SUSP0_BRK"},     {2002, "SUSP0_POS"},    {2003, "SUSP0_UP"},
    {2004, "SUSP0_DN"},
    {2011, "SUSP1_BRK"},     {2012, "SUSP1_POS"},    {2013, "SUSP1_UP"},
    {2014, "SUSP1_DN"},
    {2021, "SUSP2_BRK"},     {2022, "SUSP2_POS"},    {2023, "SUSP2_UP"},
    {2024, "SUSP2_DN"},
};

static void params_table()
{
    ImGui::SetNextWindowPos(ImVec2(838, 4), ImGuiCond_Once);
    ImGui::SetNextWindowSize(ImVec2(380, 510), ImGuiCond_Once);
    ImGui::Begin("ed_fm_get_param");

    ImGuiTableFlags fl = ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersInnerV |
                         ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY;
    if (ImGui::BeginTable("pt", 3, fl)) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Idx",   ImGuiTableColumnFlags_WidthFixed,   58.0f);
        ImGui::TableSetupColumn("Name",  ImGuiTableColumnFlags_WidthFixed,  145.0f);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        for (const auto& p : k_params) {
            double v = ed_fm_get_param(p.idx);
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0); ImGui::Text("%u", p.idx);
            ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted(p.name);
            ImGui::TableSetColumnIndex(2);
            if (v != 0.0)
                ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.5f, 1.0f), "%.5f", v);
            else
                ImGui::TextDisabled("0.00000");
        }
        ImGui::EndTable();
    }
    ImGui::End();
}

// ---- gear panel ----

static void gear_panel()
{
    ImGui::SetNextWindowPos(ImVec2(598, 518), ImGuiCond_Once);
    ImGui::SetNextWindowSize(ImVec2(620, 240), ImGuiCond_Once);
    ImGui::Begin("Gear");

    static const char* names[] = {"NOSE  (unit 0)", "LEFT  (unit 1)", "RIGHT (unit 2)"};
    static const char* pos_jsb_paths[] = {
        "/fdm/jsbsim/systems/gear/unit[0]/pos-norm",
        "/fdm/jsbsim/systems/gear/unit[1]/pos-norm",
        "/fdm/jsbsim/systems/gear/unit[2]/pos-norm",
    };
    static const unsigned idx_pos[] = {2002, 2012, 2022};
    static const unsigned idx_up[]  = {2003, 2013, 2023};
    static const unsigned idx_dn[]  = {2004, 2014, 2024};

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImGui::Columns(3, "gear_cols");

    for (int g = 0; g < 3; ++g) {
        double pos_jsb   = gprop(pos_jsb_paths[g]);
        double pos_param = ed_fm_get_param(idx_pos[g]);
        double up        = ed_fm_get_param(idx_up[g]);
        double dn        = ed_fm_get_param(idx_dn[g]);

        ImGui::Text("%s", names[g]);
        ImGui::Separator();

        ImVec2 c = ImGui::GetCursorScreenPos();
        // Two tape gauges side by side: JSBSim direct vs ParamMap
        testbed::draw_tape_gauge(dl, ImVec2(c.x + 6,  c.y + 14), 52, 68,
            (float)pos_jsb,   0.0f, 1.0f, "JSBSim", "%.2f");
        testbed::draw_tape_gauge(dl, ImVec2(c.x + 64, c.y + 14), 52, 68,
            (float)pos_param, 0.0f, 1.0f, "ParamMap", "%.2f");
        ImGui::Dummy(ImVec2(126, 92));

        ImVec4 col_dn = dn  > 0.5 ? ImVec4(0.1f, 1.0f, 0.2f, 1.0f) : ImVec4(0.35f, 0.35f, 0.35f, 1.0f);
        ImVec4 col_up = up  > 0.5 ? ImVec4(1.0f, 0.7f, 0.0f, 1.0f) : ImVec4(0.35f, 0.35f, 0.35f, 1.0f);
        ImGui::TextColored(col_dn, "DN-LOCK  %.2f", dn);
        ImGui::TextColored(col_up, "UP-LOCK  %.2f", up);

        ImGui::NextColumn();
    }

    ImGui::Columns(1);
    ImGui::End();
}

// ---- command panel ----

struct CmdEntry {
    int   icommand;
    char  name[80];
    bool  is_discrete;
    float discrete_value;
    float slider_min;
    float slider_max;
    float current;   // slider state for axis commands
};

static std::vector<CmdEntry> g_cmds;
static bool g_cmds_loaded = false;

static void try_load_commands()
{
    if (g_cmds_loaded) return;
    int n = acefm_get_command_count();
    if (n <= 0) return;
    g_cmds.resize((size_t)n);
    for (int i = 0; i < n; ++i) {
        auto& c = g_cmds[(size_t)i];
        int disc = 0;
        acefm_get_command_info(i, &c.icommand, c.name, (int)sizeof(c.name),
                               &disc, &c.discrete_value, &c.slider_min, &c.slider_max);
        c.is_discrete = (disc != 0);
        c.current = 0.0f;
    }
    g_cmds_loaded = true;
}

static void commands_panel()
{
    try_load_commands();

    ImGui::SetNextWindowPos(ImVec2(4, 518), ImGuiCond_Once);
    ImGui::SetNextWindowSize(ImVec2(590, 280), ImGuiCond_Once);
    ImGui::Begin("Commands");

    if (!g_cmds_loaded || g_cmds.empty()) {
        ImGui::TextDisabled("(no commands registered yet)");
        ImGui::End();
        return;
    }

    ImGuiTableFlags fl = ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersInnerV |
                         ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY;
    if (ImGui::BeginTable("cmdt", 4, fl)) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Command",  ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Type",     ImGuiTableColumnFlags_WidthFixed, 60.0f);
        ImGui::TableSetupColumn("Control",  ImGuiTableColumnFlags_WidthFixed, 175.0f);
        ImGui::TableSetupColumn("Value",    ImGuiTableColumnFlags_WidthFixed, 70.0f);
        ImGui::TableHeadersRow();

        for (auto& c : g_cmds) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            // Strip common "iCommand" prefix to save space
            const char* label = c.name;
            if (strncmp(label, "iCommand", 8) == 0) label += 8;
            ImGui::TextUnformatted(label);

            ImGui::TableSetColumnIndex(1);
            ImGui::TextDisabled(c.is_discrete ? "button" : "axis");

            ImGui::TableSetColumnIndex(2);
            ImGui::PushID(c.icommand);
            if (c.is_discrete) {
                if (ImGui::Button("Send", ImVec2(60, 0)))
                    ed_fm_set_command(c.icommand, c.discrete_value);
                ImGui::SameLine();
                ImGui::TextDisabled("= %.2f", c.discrete_value);
            } else {
                ImGui::SetNextItemWidth(170.0f);
                if (ImGui::SliderFloat("##s", &c.current, c.slider_min, c.slider_max, "%.3f"))
                    ed_fm_set_command(c.icommand, c.current);
            }
            ImGui::PopID();

            ImGui::TableSetColumnIndex(3);
            double rb = acefm_get_command_readback(c.icommand);
            ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "%.4f", rb);
        }
        ImGui::EndTable();
    }
    ImGui::End();
}

// ---- public API ----

void InstrumentsUpdate()
{
    if (!g_hwnd) return;

    MSG msg;
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
        if (msg.message == WM_QUIT) return;
    }

    if (!g_hwnd || !IsWindowVisible(g_hwnd)) return;

    ImGui_ImplOpenGL2_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    engine_panel();
    eadi_panel();
    params_table();
    commands_panel();
    gear_panel();

    ImGui::Render();
    glViewport(0, 0, g_w, g_h);
    glClearColor(0.08f, 0.08f, 0.08f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
    SwapBuffers(g_hdc);
}

void InstrumentsShutdown()
{
    ImGui_ImplOpenGL2_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    if (g_hrc) {
        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(g_hrc);
        g_hrc = nullptr;
    }
    if (g_hwnd && g_hdc) {
        ReleaseDC(g_hwnd, g_hdc);
        g_hdc = nullptr;
    }
    if (g_hwnd) {
        DestroyWindow(g_hwnd);
        UnregisterClassW(L"acEFM_Instr", GetModuleHandleW(nullptr));
        g_hwnd = nullptr;
    }
}
