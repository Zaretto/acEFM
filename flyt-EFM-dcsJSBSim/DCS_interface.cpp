#include "stdafx.h"
#include <Windows.h>

#include "DCS_interface.h"
#include "fm_math.h"
#include "math/FGQuaternion.h"
#include "models/FGPropulsion.h"
#include "models/FGMassBalance.h"
#include "models/FGGroundReactions.h"
#include "models/FGLGear.h"
#include "iCommands.h"   // DCS input command codes (from DCS-OpenSource/LuaToolsPlugin)
#include <string>
#include <vector>
#include <set>
#include <array>
#include <cstring>
#include <cstdarg>
#include <cassert>

using namespace DCS;     // bring iCommand* identifiers into scope
Vec3d center_of_mass;                    // m
Vec3d common_moment;                     // kg/m^2
Vec3d common_force;                      // N

extern FGJSBsim* get_model();
extern DCS_interface* get_dcs_interface();

DCS_interface::DCS_interface() : drawArguments(get_model())
{
}


DCS_interface::~DCS_interface()
{
}

/* main loop function, called every 6ms, all calculations done in here */



cockpit_param_api cockpit_param;
PFN_ED_COCKPIT_SET_DRAW_ARGUMENT ed_cockpit_set_draw_argument;
char i = 0;

int local_debug = 1;
double dT;
double ro_kgm3 = 1.225;// 1013hPa
// cached atmosphere values for debug logging
static double dbg_altitude_ft = 0;
static double dbg_temperature_R = 0;
static double dbg_speed_of_sound_fps = 0;

static LARGE_INTEGER qpc_freq;
static LARGE_INTEGER qpc_last;
static bool qpc_init = false;
static long long debug_frame_counter = 0;

int frame_count = 0;
int nullf() { return 0; }

// Last-reported mass/CG/inertia from DCS (via ed_fm_set_current_mass_state),
// stored here for reference (DCS is responsible for this state; we don't push it into JSBSim).
static double last_dcs_mass_kg          = 0;
static double last_dcs_moi_x_kgm2       = 0;
static double last_dcs_moi_y_kgm2       = 0;
static double last_dcs_moi_z_kgm2       = 0;

// Cumulative mass-change deltas we have already served to DCS via
// ed_fm_change_mass.  DCS accumulates these into its own mass state, so each
// frame we report *incremental* changes relative to what we previously served.
// Units: kg (mass), kg*m^2 (inertia deltas).
static double served_mass_delta_kg      = 0;
static double served_moi_x_delta_kgm2   = 0;
static double served_moi_y_delta_kgm2   = 0;
static double served_moi_z_delta_kgm2   = 0;

// ============================================================================
// Callback recording
// ----------------------------------------------------------------------------
// Writes every ed_fm_* callback to c:/temp/dcs-bridge-log.csv so the exact
// sequence can be replayed offline through TestPlane.exe.  Activated when
// /fdm/jsbsim/acefm/debug-record is non-zero (re-checked every 64 callbacks so
// it's cheap).  Format per line:
//
//   time_us,METHOD,param1,param2,...
//
// METHODs: ATM (atmosphere), STATE (world-frame), STATE_BODY (body-frame),
//          CMD (ed_fm_set_command), SIM (ed_fm_simulate).
// ============================================================================
static FILE*          rec_file      = nullptr;
static bool           rec_enabled   = false;
static bool           rec_suppressed = false;
static bool           rec_qpc_init  = false;
static LARGE_INTEGER  rec_qpc_freq;
static int            rec_check_ctr = 0;
static const char*    REC_LOG_PATH  = "c:/temp/dcs-bridge-log.csv";

static long long rec_now_us()
{
    if (!rec_qpc_init) {
        QueryPerformanceFrequency(&rec_qpc_freq);
        rec_qpc_init = true;
    }
    LARGE_INTEGER c;
    QueryPerformanceCounter(&c);
    return (long long)((double)c.QuadPart * 1000000.0 / (double)rec_qpc_freq.QuadPart);
}

static void rec_close()
{
    if (rec_file) { fflush(rec_file); fclose(rec_file); rec_file = nullptr; }
    rec_enabled = false;
}

// Re-evaluate the debug-record property periodically and open/close the log
// accordingly.  Returns true if recording is currently active.
static bool rec_active()
{
    if ((++rec_check_ctr & 0x3F) == 0) {
        FGJSBsim* m = get_model();
        bool want = !rec_suppressed && m
                    && (m->fgGetDouble("/fdm/jsbsim/acefm/debug-record") != 0.0);
        if (want && !rec_file) {
            rec_file = fopen(REC_LOG_PATH, "w");
            if (rec_file) {
                fprintf(rec_file, "# dcs-bridge callback log\n");
                fprintf(rec_file, "# time_us,METHOD,params...\n");
            }
            rec_enabled = (rec_file != nullptr);
        } else if (!want && rec_file) {
            rec_close();
        }
    }
    return rec_enabled && rec_file;
}

static void rec_atm(double h, double t, double a, double ro, double p,
                    double wvx, double wvy, double wvz)
{
    if (!rec_active()) return;
    fprintf(rec_file,
        "%lld,ATM,%.15g,%.15g,%.15g,%.15g,%.15g,%.15g,%.15g,%.15g\n",
        rec_now_us(), h, t, a, ro, p, wvx, wvy, wvz);
}

static void rec_state(double ax, double ay, double az,
                      double vx, double vy, double vz,
                      double px, double py, double pz,
                      double odx, double ody, double odz,
                      double ox, double oy, double oz,
                      double qx, double qy, double qz, double qw)
{
    if (!rec_active()) return;
    fprintf(rec_file,
        "%lld,STATE,%.15g,%.15g,%.15g,%.15g,%.15g,%.15g,%.15g,%.15g,%.15g,"
        "%.15g,%.15g,%.15g,%.15g,%.15g,%.15g,%.15g,%.15g,%.15g,%.15g\n",
        rec_now_us(), ax, ay, az, vx, vy, vz, px, py, pz,
        odx, ody, odz, ox, oy, oz, qx, qy, qz, qw);
}

static void rec_state_body(double ax, double ay, double az,
                           double vx, double vy, double vz,
                           double wvx, double wvy, double wvz,
                           double odx, double ody, double odz,
                           double ox, double oy, double oz,
                           double yaw, double pitch, double roll,
                           double alpha, double beta)
{
    if (!rec_active()) return;
    fprintf(rec_file,
        "%lld,STATE_BODY,%.15g,%.15g,%.15g,%.15g,%.15g,%.15g,%.15g,%.15g,%.15g,"
        "%.15g,%.15g,%.15g,%.15g,%.15g,%.15g,%.15g,%.15g,%.15g,%.15g,%.15g\n",
        rec_now_us(), ax, ay, az, vx, vy, vz, wvx, wvy, wvz,
        odx, ody, odz, ox, oy, oz, yaw, pitch, roll, alpha, beta);
}

static void rec_cmd(int command, float value)
{
    if (!rec_active()) return;
    fprintf(rec_file, "%lld,CMD,%d,%.15g\n", rec_now_us(), command, (double)value);
}

// Record what DCS tells the bridge about current mass / CG / moments of inertia
// (ed_fm_set_current_mass_state).  Units as supplied by DCS: kg, m, kg*m^2.
static void rec_mass_in(double mass,
                        double cgx, double cgy, double cgz,
                        double moix, double moiy, double moiz)
{
    if (!rec_active()) return;
    fprintf(rec_file,
        "%lld,MASS_IN,%.15g,%.15g,%.15g,%.15g,%.15g,%.15g,%.15g\n",
        rec_now_us(), mass, cgx, cgy, cgz, moix, moiy, moiz);
}

// Record what the bridge returns to DCS via ed_fm_change_mass.  Units: kg, m,
// kg*m^2.  Each record is a single "accept" iteration of the change loop.
static void rec_mass_out(double dmass,
                         double dx, double dy, double dz,
                         double dmoix, double dmoiy, double dmoiz)
{
    if (!rec_active()) return;
    fprintf(rec_file,
        "%lld,MASS_OUT,%.15g,%.15g,%.15g,%.15g,%.15g,%.15g,%.15g\n",
        rec_now_us(), dmass, dx, dy, dz, dmoix, dmoiy, dmoiz);
}

// Called from ed_fm_simulate (which lives in Aircraft.cpp).
void rec_sim(double dt)
{
    if (!rec_active()) return;
    fprintf(rec_file, "%lld,SIM,%.15g\n", rec_now_us(), dt);
    fflush(rec_file);  // flush every tick so the log is usable after a crash
}

// ============================================================================
// Entry-point call tracing
// ----------------------------------------------------------------------------
// Logs every ed_fm_* entry point DCS calls, in order, so the host's execution
// order and call frequency can be established from a real session rather than
// inferred from the headers.
//
// Entries are buffered in memory from the very first call, because DCS calls
// ed_fm_set_plugin_data_install_path and ed_fm_configure before the JSBSim model
// (and therefore the property tree) exists. The buffer is resolved on the first
// ed_fm_simulate, when /fdm/jsbsim/acefm/debug-calls decides its fate:
//
//   0     off; the buffer is discarded (default)
//   N > 0 log every call for the first N frames, then log only what is new
//   -1    log every call for the whole session (large: get_param alone runs to
//         tens of calls per frame)
//
// The N > 0 form is the useful one for a whole sortie. The first N frames
// establish the steady-state call order; after that the per-frame traffic is
// already known and only novelty matters, so a call survives only if:
//
//   * its name has not been seen before (ed_fm_release, the start hooks,
//     ed_fm_on_damage and the rest first appear long after frame N), or
//   * it is ed_fm_get_param with an index not asked for before, which builds the
//     full set of parameters DCS actually wants, or
//   * its name is event-driven rather than per-frame (below), where every
//     occurrence carries information.
//
// Line format: seq,time_us,frame,name,detail
// ============================================================================
static const char*  TRC_LOG_PATH = "c:/temp/acefm-calltrace.csv";
static const size_t TRC_BUFFER_MAX = 8192;

// Names worth logging every time they occur, even after the frame limit: they
// fire on events rather than per frame, so each call is a distinct fact. The
// exception is ed_fm_suspension_feedback, which is per strut per frame but only
// while there is ground contact, which is exactly what we are trying to see.
static const char* TRC_ALWAYS[] = {
    "ed_fm_suspension_feedback",
    "ed_fm_set_command",
    "ed_fm_on_damage",
    "ed_fm_on_planned_failure",
    "ed_fm_set_property_numeric",
    "ed_fm_set_property_string",
    "ed_fm_push_simulation_event",
    "ed_fm_refueling_add_fuel",
    "ed_fm_set_internal_fuel",
    "ed_fm_set_external_fuel",
    "ed_fm_release",
    "ed_fm_repair",
};

static FILE*                    trc_file = nullptr;
static std::vector<std::string> trc_buffer;
static std::set<std::string>    trc_seen;      // names, and get_param indices
static std::map<unsigned,double> trc_last_param;  // last logged value, gear block only
static long long                trc_seq = 0;
static long long                trc_frame_no = 0;
static long long                trc_dropped = 0;
static int                      trc_limit = 0;

enum TrcState { TRC_UNRESOLVED, TRC_ON, TRC_NOVEL, TRC_OFF };
static TrcState trc_state = TRC_UNRESOLVED;

// Should this call be logged now that the frame limit has passed?
static bool trc_is_novel(const char* name, const char* detail)
{
    for (const char* always : TRC_ALWAYS)
        if (strcmp(name, always) == 0) return true;

    if (detail && strcmp(name, "ed_fm_get_param") == 0) {
        unsigned index = 0;
        double   value = 0;
        if (sscanf(detail, "index=%u value=%lf", &index, &value) == 2) {
            // The gear block is what a ground test is about, and it only changes
            // when the pilot brakes or steers, so log every change of it. The rest
            // (engine RPM, temperatures) changes every frame and would drown the
            // file, so record only the first time DCS asks for each index.
            const bool gear = index >= ED_FM_SUSPENSION_0_RELATIVE_BRAKE_MOMENT
                           && index <= ED_FM_ANTI_SKID_ENABLE;
            if (gear) {
                auto it = trc_last_param.find(index);
                if (it != trc_last_param.end() && it->second == value) return false;
                trc_last_param[index] = value;
                return true;
            }
        }
        // Keyed on the index alone: the value changes and must not defeat this.
        std::string key(name);
        key += detail;
        const char* space = strchr(detail, ' ');
        if (space) key = std::string(name) + std::string(detail, space - detail);
        return trc_seen.insert(key).second;
    }

    return trc_seen.insert(std::string(name)).second;
}

static void trc_emit(const char* name, const char* detail)
{
    if (trc_state == TRC_OFF) return;

    if (trc_state == TRC_NOVEL && !trc_is_novel(name, detail)) {
        ++trc_dropped;
        return;
    }

    char line[512];
    snprintf(line, sizeof(line), "%lld,%lld,%lld,%s,%s\n",
             ++trc_seq, rec_now_us(), trc_frame_no, name, detail ? detail : "");

    if (trc_state == TRC_UNRESOLVED) {
        if (trc_buffer.size() < TRC_BUFFER_MAX) trc_buffer.push_back(line);
    } else if (trc_file) {
        fputs(line, trc_file);
    }
}

void trace_call(const char* name)
{
    trc_emit(name, nullptr);
}

void trace_callf(const char* name, const char* fmt, ...)
{
    if (trc_state == TRC_OFF) return;

    char detail[384];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(detail, sizeof(detail), fmt, ap);
    va_end(ap);

    trc_emit(name, detail);
}

// Called at the top of every ed_fm_simulate, where the model is guaranteed to
// exist. Resolves the buffered pre-init calls on the first frame and enforces
// the frame limit thereafter.
void trace_frame(void)
{
    ++trc_frame_no;

    if (trc_state == TRC_UNRESOLVED) {
        FGJSBsim* m = get_model();
        trc_limit = m ? (int)m->fgGetDouble("/fdm/jsbsim/acefm/debug-calls") : 0;

        if (trc_limit != 0) trc_file = fopen(TRC_LOG_PATH, "w");
        if (trc_file) {
            fprintf(trc_file, "# acEFM entry-point call trace\n");
            fprintf(trc_file, "seq,time_us,frame,name,detail\n");
            for (const auto& line : trc_buffer) fputs(line.c_str(), trc_file);
            fflush(trc_file);
            trc_state = TRC_ON;
        } else {
            trc_state = TRC_OFF;
        }

        trc_buffer.clear();
        trc_buffer.shrink_to_fit();
    }

    if (trc_state == TRC_OFF || !trc_file) return;

    // Flush per frame rather than per call: the session is long and a crash must
    // not lose the frame that caused it.
    fflush(trc_file);

    // Once the frame limit passes, the steady-state call order is established.
    // Keep the file open for the rest of the session but log only novelty, so a
    // whole sortie stays small enough to read.
    if (trc_state == TRC_ON && trc_limit > 0 && trc_frame_no > trc_limit) {
        fprintf(trc_file,
                "# %d-frame call order established; logging only new calls,"
                " new ed_fm_get_param indices and event-driven calls from here\n",
                trc_limit);
        trc_state = TRC_NOVEL;
    }
}

// Close the trace cleanly. Called from ed_fm_release, which is the only point
// at which DCS tells us the aircraft is finished with.
void trace_close(void)
{
    if (!trc_file) return;
    fprintf(trc_file, "# session end: %lld calls logged, %lld repeats dropped\n",
            trc_seq, trc_dropped);
    fclose(trc_file);
    trc_file = nullptr;
    trc_state = TRC_OFF;
}

static std::map<std::string, double> conversion_map = {
{"METER_TO_FEET", 3.28084},
{"FEET_TO_METER  ", 0.3048},
{"DEGREES_TO_RADIANS", 0.0174532925},
{"RADIANS_TO_DEGREES", 57.295779505},
{"ZERO", 0.0000001}, // avoid division by zero
{"LBSFT_TO_NM", 1.3558179483314},
{"LBS_TO_N", 4.4483985765124555160142348754448},
{"KGM3_TO_SLUGS_FT3", 0.0019403203},
{"INCHES_TO_METERS", 0.0254},
{"SLUG_FT2_TO_KG_M2", 1.35594},
{"PA_TO_LBF_FT2", 0.02088527017024426350851221317543}
};


// Build default ed_fm_get_param bindings for N engines using standard JSBSim
// turbine property paths.  Called after XML <params> are parsed so any
// aircraft-specific <param> overrides already registered by AddItem() take
// priority (AddDefault skips indices that are already present).
//
// Engine count from <sim><engine-count> in aceFMconfig.xml; if absent or zero,
// falls back to however many engines JSBSim loaded.
//
// Default mappings (override per-aircraft via <params> in aceFMconfig.xml):
//   RPM              -> n1  (LP/fan spool %)      factor 1
//   RELATED_RPM      -> n2  (HP/core spool, 0-1)  factor 0.01
//   CORE_RPM         -> n2  (HP/core spool %)      factor 1
//   CORE_RELATED_RPM -> n2  (0-1)                  factor 0.01
//   THRUST           -> thrust-lbs                 factor LBS_TO_N
//   TEMPERATURE      -> egt-degc                   factor 1
//   OIL_PRESSURE     -> oil-pressure-psi           factor 1
//   FUEL_FLOW        -> fuel-flow-rate-pps          factor 1
//   COMBUSTION       -> running                     factor 1
static void RegisterEngineDefaults(FGJSBsim* model, ParamMap& params, int engineCount)
{
    static const unsigned BLOCK = ED_FM_ENGINE_1_RPM - ED_FM_ENGINE_0_RPM;
    printf("acEFM: RegisterEngineDefaults for %d engine(s)\n", engineCount);
    for (int jsb = 0; jsb < engineCount; jsb++) {
        unsigned base = (unsigned)(jsb + 1) * BLOCK; // DCS engine jsb+1 (0 = APU)
        std::string p = "/fdm/jsbsim/propulsion/engine[" + std::to_string(jsb) + "]/";

        params.AddDefault(model, base + ED_FM_ENGINE_0_RPM,              p + "n1",                  1.0);
        params.AddDefault(model, base + ED_FM_ENGINE_0_RELATED_RPM,      p + "n2",                  0.01);
        params.AddDefault(model, base + ED_FM_ENGINE_0_CORE_RPM,         p + "n2",                  1.0);
        params.AddDefault(model, base + ED_FM_ENGINE_0_CORE_RELATED_RPM, p + "n2",                  0.01);
        params.AddDefault(model, base + ED_FM_ENGINE_0_THRUST,           p + "thrust-lbs",          LBS_TO_N);
        params.AddDefault(model, base + ED_FM_ENGINE_0_TEMPERATURE,      p + "egt-degc",            1.0);
        params.AddDefault(model, base + ED_FM_ENGINE_0_OIL_PRESSURE,     p + "oil-pressure-psi",    1.0);
        params.AddDefault(model, base + ED_FM_ENGINE_0_FUEL_FLOW,        p + "fuel-flow-rate-pps",  1.0);
        params.AddDefault(model, base + ED_FM_ENGINE_0_COMBUSTION,       p + "running",             1.0);
    }
}

// Build default ed_fm_get_param bindings for the gear, using standard JSBSim
// property paths.  Called after XML <params> so aircraft overrides win.
//
// DCS is responsible for the suspension, but it cannot solve it without knowing what the gear
// is doing, so it polls us for the state of every strut each frame.  Answering
// zero to the brake moment means the aircraft cannot be stopped.
//
// The brake mapping does not need configuring, because JSBSim already holds it.
// Each gear unit knows its own brake group (FGLGear::GetBrakeGroup), and FGFCS
// ties the three brake channels, so walking the units gives the binding.
//
//   GEAR_POST_STATE       -> gear/unit[N]/pos-norm  (0 retracted, 1 extended)
//   RELATIVE_BRAKE_MOMENT -> the brake channel of unit N's own group
//   WHEEL_YAW             -> fcs/steer-pos-deg[N], for a steerable unit
//
// DCS polls two brake paths, so both are answered from the same channels:
// the per-strut suspension brake moment, and the FC3 wheel brakes.
//
// Not defaulted, because JSBSim has no standard source: ANTI_SKID_ENABLE, the
// brake chute, and WHEEL_SELF_ATTITUDE.  Supply them per aircraft with <params>.
//
// UP_LOCK and DOWN_LOCK are not defaulted either: an instrumented sortie shows
// DCS never asks for them.
//
// Gear unit count from <sim><gear-count>; defaults to 3 (tricycle) if absent.
static void RegisterGearDefaults(FGJSBsim* model, ParamMap& params, int gearCount)
{
    static const unsigned BLOCK      = ED_FM_SUSPENSION_1_RELATIVE_BRAKE_MOMENT
                                     - ED_FM_SUSPENSION_0_RELATIVE_BRAKE_MOMENT;
    static const unsigned POST_STATE = ED_FM_SUSPENSION_0_GEAR_POST_STATE
                                     - ED_FM_SUSPENSION_0_RELATIVE_BRAKE_MOMENT;
    static const unsigned WHEEL_YAW  = ED_FM_SUSPENSION_0_WHEEL_YAW
                                     - ED_FM_SUSPENSION_0_RELATIVE_BRAKE_MOMENT;

    // FGLGear::BrakeGroup -> the FGFCS channel that drives it.  A unit in no brake
    // group gets no binding and reads zero, which is what DCS should hear for an
    // unbraked wheel.
    //
    // NOTE: JSBSim maps both NOSE and TAIL onto bgCenter (FGLGear.cpp, "Nose brake
    // is not supported by FGFCS"), so a nose wheel declared <brake_group>NOSE
    // reports itself as centre-braked and picks up fcs/center-brake-cmd-norm here.
    // That is faithful to what JSBSim believes, and harmless while nothing drives
    // the centre channel, but an aircraft that does use it would find DCS braking
    // its nose wheel.  Override that strut with <params> if it matters.
    auto brake_channel = [](int group) -> const char* {
        switch (group) {
        case JSBSim::FGLGear::bgLeft:   return "/fdm/jsbsim/fcs/left-brake-cmd-norm";
        case JSBSim::FGLGear::bgRight:  return "/fdm/jsbsim/fcs/right-brake-cmd-norm";
        case JSBSim::FGLGear::bgCenter: return "/fdm/jsbsim/fcs/center-brake-cmd-norm";
        default:                        return nullptr;
        }
    };

    printf("acEFM: RegisterGearDefaults for %d unit(s)\n", gearCount);

    const int units = model->GroundReactions->GetNumGearUnits();

    for (int n = 0; n < gearCount; n++) {
        unsigned base = ED_FM_SUSPENSION_0_RELATIVE_BRAKE_MOMENT + (unsigned)n * BLOCK;
        std::string p = "/fdm/jsbsim/gear/unit[" + std::to_string(n) + "]/";

        params.AddDefault(model, base + POST_STATE, p + "pos-norm", 1.0);

        if (n >= units) continue;   // DCS has more struts than JSBSim models

        auto gear = model->GroundReactions->GetGearUnit(n);

        // RELATIVE_BRAKE_MOMENT is a fraction of the strut's maximum brake moment,
        // which is what the JSBSim brake command already is.  It sits at the base
        // of each suspension block, so its index is `base` itself.
        if (const char* channel = brake_channel(gear->GetBrakeGroup()))
            params.AddDefault(model, base, channel, 1.0);

        // Steering angle.  NOTE: the units DCS wants for WHEEL_YAW are not stated
        // in the API, so this is degrees until an instrumented taxi says otherwise.
        if (gear->GetSteerable())
            params.AddDefault(model, base + WHEEL_YAW,
                              "/fdm/jsbsim/fcs/steer-pos-deg[" + std::to_string(n) + "]",
                              1.0);
    }

    // The FC3 wheel brakes, which DCS polls alongside the per-strut brake moment.
    params.AddDefault(model, ED_FM_FC3_WHEEL_BRAKE_LEFT,
                      "/fdm/jsbsim/fcs/left-brake-cmd-norm", 1.0);
    params.AddDefault(model, ED_FM_FC3_WHEEL_BRAKE_RIGHT,
                      "/fdm/jsbsim/fcs/right-brake-cmd-norm", 1.0);
    params.AddDefault(model, ED_FM_FC3_WHEEL_BRAKE_COMMAND_LEFT,
                      "/fdm/jsbsim/fcs/left-brake-cmd-norm", 1.0);
    params.AddDefault(model, ED_FM_FC3_WHEEL_BRAKE_COMMAND_RIGHT,
                      "/fdm/jsbsim/fcs/right-brake-cmd-norm", 1.0);
}

void DCS_interface::initialize(FGJSBsim * _model)
{
    model = _model;
    printf("acEFM: DCS_interface Initialize\n");

    auto drawArgNode = _model->PropertyManager->GetNode("sim/animations");
    if (drawArgNode != nullptr) {
        for (auto drawarg : drawArgNode->getChildren("drawarg")) {
            auto prop = drawarg->getStringValue("property");
            auto factor = drawarg->getFloatValue("factor", 1.0);
            auto delta = drawarg->getFloatValue("delta", 0.0001);
            auto offset = drawarg->getFloatValue("offset", 0);
            drawArguments.AddItem(drawarg->getIndex(), prop, factor, delta, offset);
        }
    }

    auto paramsNode = _model->PropertyManager->GetNode("sim/params");
    if (paramsNode != nullptr) {
        static const std::map<std::string, int> enumLookup(
            enumMap, enumMap + sizeof(enumMap) / sizeof(enumMap[0]));
        for (auto param : paramsNode->getChildren("param")) {
            std::string indexStr = param->getStringValue("index");
            unsigned idx;
            auto it = enumLookup.find(indexStr);
            if (it != enumLookup.end()) {
                idx = (unsigned)it->second;
            } else {
                try { idx = (unsigned)std::stoi(indexStr); }
                catch (...) {
                    SG_LOG(SG_FLIGHT, SG_ALERT, "Param:: unknown index '" << indexStr << "'");
                    continue;
                }
            }
            std::string prop = param->getStringValue("property");
            double factor = param->getDoubleValue("factor", 1.0);
            double offset = param->getDoubleValue("offset", 0.0);
            if (!prop.empty())
                params.AddItem(_model, idx, prop, factor, offset);
        }
    }

    {
        SGPropertyNode* ecNode = _model->PropertyManager->GetNode("sim/engine-count");
        int engineCount = (ecNode && ecNode->getDoubleValue() > 0)
                          ? (int)ecNode->getDoubleValue()
                          : (int)_model->Propulsion->GetNumEngines();
        RegisterEngineDefaults(_model, params, engineCount);
    }

    {
        SGPropertyNode* gcNode = _model->PropertyManager->GetNode("sim/gear-count");
        int gearCount = (gcNode && gcNode->getDoubleValue() > 0)
                        ? (int)gcNode->getDoubleValue()
                        : 3;
        RegisterGearDefaults(_model, params, gearCount);
    }

    {
        auto ecsNode = _model->PropertyManager->GetNode("sim/ecs");
        if (ecsNode) {
            std::string path = ecsNode->getStringValue("cabin-altitude-ft");
            if (!path.empty())
                cabin_altitude_node = _model->PropertyManager->GetNode(path);
        }
    }

    auto cockpitNode = _model->PropertyManager->GetNode("sim/cockpit");
    if (cockpitNode != nullptr) {
        // cockpit base dll
        HMODULE cockpit_base_dll = GetModuleHandle(L"CockpitBase.dll"); //assume that we work inside same process
        if (cockpit_base_dll == NULL)

        {
            std::cout << "cockpit base dll not found\n";
            cockpit_param.pfn_ed_cockpit_get_parameter_handle = (PFN_ED_COCKPIT_GET_PARAMETER_HANDLE)nullf;
            cockpit_param.pfn_ed_cockpit_update_parameter_with_number = (PFN_ED_COCKPIT_UPDATE_PARAMETER_WITH_NUMBER)nullf;
            cockpit_param.pfn_ed_cockpit_update_parameter_with_string = (PFN_ED_COCKPIT_UPDATE_PARAMETER_WITH_STRING)nullf;
            cockpit_param.pfn_ed_cockpit_parameter_value_to_number = (PFN_ED_COCKPIT_PARAMETER_VALUE_TO_NUMBER)nullf;
            cockpit_param.pfn_ed_cockpit_parameter_value_to_string = (PFN_ED_COCKPIT_PARAMETER_VALUE_TO_STRING)nullf;
        } else {
            std::cout << "found cockpit base dll\n";
            cockpit_param.pfn_ed_cockpit_get_parameter_handle = (PFN_ED_COCKPIT_GET_PARAMETER_HANDLE)GetProcAddress(cockpit_base_dll, "ed_cockpit_get_parameter_handle");
            cockpit_param.pfn_ed_cockpit_update_parameter_with_number = (PFN_ED_COCKPIT_UPDATE_PARAMETER_WITH_NUMBER)GetProcAddress(cockpit_base_dll, "ed_cockpit_update_parameter_with_number");
            cockpit_param.pfn_ed_cockpit_update_parameter_with_string = (PFN_ED_COCKPIT_UPDATE_PARAMETER_WITH_STRING)GetProcAddress(cockpit_base_dll, "ed_cockpit_update_parameter_with_string");
            cockpit_param.pfn_ed_cockpit_parameter_value_to_number = (PFN_ED_COCKPIT_PARAMETER_VALUE_TO_NUMBER)GetProcAddress(cockpit_base_dll, "ed_cockpit_parameter_value_to_number");
            cockpit_param.pfn_ed_cockpit_parameter_value_to_string = (PFN_ED_COCKPIT_PARAMETER_VALUE_TO_STRING)GetProcAddress(cockpit_base_dll, "ed_cockpit_parameter_value_to_string");
            ed_cockpit_set_draw_argument = (PFN_ED_COCKPIT_SET_DRAW_ARGUMENT)GetProcAddress(cockpit_base_dll, "ed_cockpit_set_draw_argument");

            std::cout << "trying to get test param handle wth function at:\n";
            std::cout << cockpit_param.pfn_ed_cockpit_get_parameter_handle << "\n";

            if (cockpit_param.pfn_ed_cockpit_get_parameter_handle == NULL) {
                std::cout << GetLastError() << "\n";
            }
        }
        //TODO: make this part of the constructor and possible include all of the cockpit.dll setup from above.
        cockpit.Connect(cockpit_param);

        for (auto gauge : cockpitNode->getChildren("gauge")) {
            auto param = gauge->getStringValue("param");
            auto prop = gauge->getStringValue("property");
            auto factor = gauge->getDoubleValue("factor", 1.0);
            auto delta = gauge->getDoubleValue("delta", 0);
            if (factor == 0) { // factor of 0 makes no sense so it is probably a string lookup
                auto factor_name = gauge->getStringValue("factor");
                if (conversion_map.find(factor_name) != conversion_map.end())
                    factor = conversion_map[factor_name];
                else
                    SG_LOG(SG_FLIGHT, SG_ALERT, "Unknown factor " << factor_name << "for gauge param " << param);
            }
            auto type = gauge->getStringValue("type");

            if (type != std::string()) {
                if (type == std::string("GenevaDrive") || type == std::string("LinearDrive")) {
                    cockpit.AddItem(new LinearActuator(_model, cockpit.FindHandle(param), param, prop, type, factor, delta));
                }
                else
                    SG_LOG(SG_FLIGHT, SG_ALERT, "Unknown gauge type " << type);
            } else {
                if (param == std::string())
                    SG_LOG(SG_FLIGHT, SG_ALERT, "Gauge param missing");
                if (prop == std::string())
                    SG_LOG(SG_FLIGHT, SG_ALERT, "Gauge " << param << " requires property");
                cockpit.AddItem(_model, param, prop, factor);
            }
        }
    } else
        SG_LOG(SG_FLIGHT, SG_ALERT, "Missing cockpit definition in /sim/cockpit");

    // Config-driven input command bindings: map DCS iCommand codes to JSBSim
    // properties (e.g. flaps/airbrake on/off).  See <sim><commands> in
    // aceFMconfig.xml.
    auto commandsNode = _model->PropertyManager->GetNode("sim/commands");
    if (commandsNode != nullptr) {
        commands.Init(_model);
        for (auto cmd : commandsNode->getChildren("command")) {
            // Command may be given numerically (<icommand>) or by symbolic name
            // (<name>, e.g. iCommandPlaneFlapsOn) resolved via iCommands.h.
            int icmd = cmd->getIntValue("icommand", -1);
            std::string display_name = cmd->getStringValue("name");
            if (icmd < 0) {
                if (!display_name.empty())
                    icmd = DCS::iCommandFromName(display_name);
            }
            if (icmd < 0) {
                SG_LOG(SG_FLIGHT, SG_ALERT,
                       "Command binding requires a valid <icommand> or <name>");
                continue;
            }
            if (display_name.empty())
                display_name = std::to_string(icmd);

            auto prop = cmd->getStringValue("property");
            if (prop == std::string()) {
                SG_LOG(SG_FLIGHT, SG_ALERT,
                       "Command " << icmd << " requires a <property>");
                continue;
            }

            double factor = cmd->getDoubleValue("factor", 1.0);
            if (factor == 0) { // 0 makes no sense for a multiplier: treat as a named constant
                auto factor_name = cmd->getStringValue("factor");
                if (conversion_map.find(factor_name) != conversion_map.end())
                    factor = conversion_map[factor_name];
                else
                    SG_LOG(SG_FLIGHT, SG_ALERT,
                           "Unknown factor " << factor_name << " for command " << icmd);
            }
            double offset = cmd->getDoubleValue("offset", 0.0);

            // Optional output clip.  Absent bound -> no limit on that side.
            double clipMin = cmd->getDoubleValue("clip-min", -std::numeric_limits<double>::max());
            double clipMax = cmd->getDoubleValue("clip-max",  std::numeric_limits<double>::max());

            // A <value> element means "write this fixed value" (discrete
            // commands); its absence means "transform the command value".
            bool   hasFixedValue = cmd->hasChild("value");
            double fixedValue    = cmd->getDoubleValue("value", 0.0);

            // A <toggle> element means "flip the property 0<->1 once per
            // press", where its value is the threshold the incoming command
            // value must reach to count as pressed (handles both 0..1..0 and
            // 0..100..0 style pulses/ramps). Takes precedence over <value>.
            bool   isToggle        = cmd->hasChild("toggle");
            double toggleThreshold = cmd->getDoubleValue("toggle", 1.0);

            commands.AddItem(_model, icmd, display_name, prop, factor, offset, clipMin, clipMax,
                             hasFixedValue, fixedValue, isToggle, toggleThreshold);
        }
    }
}
void DCS_interface::simulate(double dt)
{
    dT = dt;
    FGJSBsim *model = get_model();
    frame_count++;
    if (frame_count > 50)
    {
        local_debug = model->fgGetDouble("/fdm/jsbsim/acefm/debug");
        //use_metric_forces = model->fgGetDouble("/fdm/jsbsim/acefm/metric_forces");
        //use_metric_moments = model->fgGetDouble("/fdm/jsbsim/acefm/metric_moments");
        frame_count = 0;
    }

    if (cockpit_param.pfn_ed_cockpit_update_parameter_with_number)
    {
        cockpit.Update(model);
    }

    // --- debug instrumentation ---
    if (local_debug) {
        debug_frame_counter++;

        // high-resolution wall clock timing
        LARGE_INTEGER qpc_now;
        QueryPerformanceCounter(&qpc_now);
        if (!qpc_init) {
            QueryPerformanceFrequency(&qpc_freq);
            qpc_last = qpc_now;
            qpc_init = true;
        }
        double wall_dt_us = (double)(qpc_now.QuadPart - qpc_last.QuadPart) * 1000000.0 / (double)qpc_freq.QuadPart;
        double wallclock_us = (double)qpc_now.QuadPart * 1000000.0 / (double)qpc_freq.QuadPart;
        qpc_last = qpc_now;

        // timing
        model->dbg.dt_dcs->setDoubleValue(dt);
        model->dbg.wallclock_us->setDoubleValue(wallclock_us);
        model->dbg.wall_dt_us->setDoubleValue(wall_dt_us);
        model->dbg.frame_count->setDoubleValue((double)debug_frame_counter);

        // forces (raw JSBSim lbs)
        model->dbg.fbx->setDoubleValue(model->dbg.src_fbx->getDoubleValue());
        model->dbg.fby->setDoubleValue(model->dbg.src_fby->getDoubleValue());
        model->dbg.fbz->setDoubleValue(model->dbg.src_fbz->getDoubleValue());

        // moments (raw JSBSim lbsft)
        model->dbg.ml->setDoubleValue(model->dbg.src_ml->getDoubleValue());
        model->dbg.mm->setDoubleValue(model->dbg.src_mm->getDoubleValue());
        model->dbg.mn->setDoubleValue(model->dbg.src_mn->getDoubleValue());

        // engine state - all engines
        unsigned int nEngines = model->Propulsion->GetNumEngines();
        model->dbg.num_engines->setDoubleValue((double)nEngines);
        for (unsigned int e = 0; e < nEngines && e < MAX_ENGINES; e++) {
            model->dbg.throttle[e]->setDoubleValue(model->dbg.src_throttle[e]->getDoubleValue());
            model->dbg.thrust[e]->setDoubleValue(model->dbg.src_thrust[e]->getDoubleValue());
            model->dbg.n1[e]->setDoubleValue(model->dbg.src_n1[e]->getDoubleValue());
        }

        // atmosphere
        model->dbg.density_slugft3->setDoubleValue(model->get_atmosphere_rho_slugs_ft3());
        model->dbg.pressure_lbfft2->setDoubleValue(model->get_atmosphere_pressure_lbf_ft2());
        model->dbg.altitude_ft->setDoubleValue(dbg_altitude_ft);
        model->dbg.temperature_R->setDoubleValue(dbg_temperature_R);
        model->dbg.speed_of_sound_fps->setDoubleValue(dbg_speed_of_sound_fps);
        // note: aero state (alpha, beta, qbar, vt, p/q/r, pitch/roll/yaw)
        // is populated by JSBSim_interface::set_current_state_body_axis()
    }
}

double ed_fm_get_param(unsigned index)
{
    DCS_interface* dcs = get_dcs_interface();
    double paramValue = 0;
    if (!dcs->params.TryGet(index, paramValue)) {
        if (index == ED_FM_COCKPIT_PRESSURIZATION_OVER_EXTERNAL) {
            if (!dcs->cabin_altitude_node) return 0;
            double cabin_alt = dcs->cabin_altitude_node->getDoubleValue();
            double cabin_pressure = dcs->model->get_pressure_at_atltiude_lbf_ft2(cabin_alt);
            return (cabin_pressure - dcs->model->get_atmosphere_pressure_lbf_ft2()) / PA_TO_LBF_FT2;
        }
    }
    if (local_debug >= 2)
        printf(" ed_fm_get_param %d = %f\n", index, paramValue);

    // Trace the value, not just the index: what DCS is told is the whole point,
    // and an unbound parameter answering zero looks identical to a bound one.
    trace_callf("ed_fm_get_param", "index=%u value=%.6g", index, paramValue);

    return paramValue;
}


extern "C" __declspec(dllexport) double acefm_get_property(const char* path)
{
    FGJSBsim* m = get_model();
    return m ? m->fgGetDouble(path) : 0.0;
}

extern "C" __declspec(dllexport) int acefm_get_command_count()
{
    DCS_interface* dcs = get_dcs_interface();
    return dcs ? (int)dcs->commands.GetRegistry().size() : 0;
}

extern "C" __declspec(dllexport) void acefm_get_command_info(
    int index, int* out_icommand, char* out_name, int name_max,
    int* out_discrete, float* out_value, float* out_min, float* out_max)
{
    DCS_interface* dcs = get_dcs_interface();
    if (!dcs) return;
    const auto& reg = dcs->commands.GetRegistry();
    if (index < 0 || index >= (int)reg.size()) return;
    const auto& e = reg[index];
    if (out_icommand) *out_icommand = e.icommand;
    if (out_name && name_max > 0) {
        strncpy(out_name, e.name.c_str(), (size_t)(name_max - 1));
        out_name[name_max - 1] = '\0';
    }
    if (out_discrete) *out_discrete = e.is_discrete ? 1 : 0;
    if (out_value)    *out_value    = e.discrete_value;
    if (out_min)      *out_min      = e.clip_min;
    if (out_max)      *out_max      = e.clip_max;
}

extern "C" __declspec(dllexport) double acefm_get_command_readback(int icommand)
{
    DCS_interface* dcs = get_dcs_interface();
    return dcs ? dcs->commands.Readback(icommand) : 0.0;
}

// Disable the bridge-callback recorder for this process regardless of the
// acefm/debug-record property. TestPlane's replay and check modes call this so
// a replayed session can never truncate the recording it is reading: the
// recorder writes to the same fixed path the replay reads from.
extern "C" __declspec(dllexport) void acefm_suppress_debug_record(void)
{
    rec_suppressed = true;
    rec_close();
}


// We provide the aerodynamic and propulsive force to the DCS integration. This
// does not include gear forces (gear is retracted as DCS calculates this)
// So we just provide the sum of these forces Name rather than using JSBSim's
// computed value.
//
// ed_fm_add_local_force/ed_fm_add_local_moment run every physics step, so the
// property nodes are resolved once (function-local statics) and read by
// pointer thereafter instead of doing a property-tree name lookup per frame.
struct AxisNodes
{
    SGPropertyNode* aero;
    SGPropertyNode* prop;
    SGPropertyNode* buoy;
    double Sum() const { return aero->getDoubleValue() + prop->getDoubleValue() + buoy->getDoubleValue(); }
};

// axis_letters supplies the three property-path axis codes in index order,
// e.g. "xyz" for forces (indices 0,1,2 -> x,y,z) or "lmn" for moments
// (indices 0,1,2 -> l,m,n).
static std::array<AxisNodes, 3> resolve_axis_nodes(FGJSBsim* model, const char* base, const char* axis_letters, const char* unit_suffix)
{
    assert(std::strlen(axis_letters) == 3 && "axis_letters must supply exactly 3 axis codes");
    std::array<AxisNodes, 3> nodes;
    for (int i = 0; i < 3; ++i) {
        const std::string axis(1, axis_letters[i]);
        nodes[i] = {
            model->PropertyManager->GetNode(std::string(base) + axis + "-aero-" + unit_suffix, true),
            model->PropertyManager->GetNode(std::string(base) + axis + "-prop-" + unit_suffix, true),
            model->PropertyManager->GetNode(std::string(base) + axis + "-buoyancy-" + unit_suffix, true)
        };
    }
    return nodes;
}

static double force_to_dcs(FGJSBsim* model, int axis)
{
    static std::array<AxisNodes, 3> nodes = resolve_axis_nodes(model, "/fdm/jsbsim/forces/fb", "xyz", "lbs");
    return nodes[axis].Sum();
}

static double moment_to_dcs(FGJSBsim* model, int axis)
{
    static std::array<AxisNodes, 3> nodes = resolve_axis_nodes(model, "/fdm/jsbsim/moments/", "lmn", "lbsft");
    return nodes[axis].Sum();
}

// Indices into the "xyz"/"lmn" resolve_axis_nodes() tables above.
enum { AXIS_X = 0, AXIS_Y = 1, AXIS_Z = 2 };
enum { AXIS_L = 0, AXIS_M = 1, AXIS_N = 2 };

void ed_fm_add_local_force(double &x, double &y, double &z, double &pos_x, double &pos_y, double &pos_z)
{
    trace_call("ed_fm_add_local_force");
    FGJSBsim *model = get_model();
    x =  force_to_dcs(model, AXIS_X) * LBS_TO_N;
    y = -force_to_dcs(model, AXIS_Z) * LBS_TO_N;
    z =  force_to_dcs(model, AXIS_Y) * LBS_TO_N;

    pos_x = center_of_mass.x;
    pos_y = center_of_mass.y;
    pos_z = center_of_mass.z;
}

void ed_fm_add_local_moment(double &x, double &y, double &z)
{
    trace_call("ed_fm_add_local_moment");
    FGJSBsim *model = get_model();

    // jsb
    // x + back
    // y + right
    // z + up

    // dcs
    // x + forward
    // y + up
    // z + right

    x =  moment_to_dcs(model, AXIS_L) * LBSFT_TO_NM;
    y = -moment_to_dcs(model, AXIS_N) * LBSFT_TO_NM;
    z =  moment_to_dcs(model, AXIS_M) * LBSFT_TO_NM;
}


//{
//    FGJSBsim* model = get_model();
//
//    // Convert units
//    double density_slugs_ft3 = ro_kgm3 * KGM3_TO_SLUGS_FT3;
//    double pressure_psf = p_Pa * PA_TO_LBF_FT2;
//    double soundspeed_fps = a * METERS_TO_FEET;
//    double altitude_ft = h * METERS_TO_FEET;
//    double temp_rankine = t * 1.8; // Kelvin to Rankine
//
//    // Set directly via property manager
//    model->PropertyManager->GetNode()->setDoubleValue("/atmosphere/rho-slugs_ft3", density_slugs_ft3);
//    model->PropertyManager->GetNode()->setDoubleValue("/atmosphere/P-psf", pressure_psf);
//    model->PropertyManager->GetNode()->setDoubleValue("/atmosphere/T-R", temp_rankine);
//    model->PropertyManager->GetNode()->setDoubleValue("/atmosphere/a-fps", soundspeed_fps);
//
//    // Also set altitude input for JSBSim
//    model->Atmosphere->in.altitudeASL = altitude_ft;
//
//    // IMPORTANT: Don't call Atmosphere->Run()!
//    // We're bypassing the calculation and injecting values directly
//}
void ed_fm_set_atmosphere(double h,	//altitude above sea level
    double t,	//Ambient temperature (kelvin)
    double a,	//speed of sound
    double _ro_kgm3,	// (kg/m^3)
    double p_Pa,	// atmosphere pressure (Pa == N/m^2)
    double wind_vx_ms,	//components of velocity vector, including turbulence in world coordinate system
    double wind_vy_ms,	//components of velocity vector, including turbulence in world coordinate system
    double wind_vz_ms	//components of velocity vector, including turbulence in world coordinate system
)
{
    trace_callf("ed_fm_set_atmosphere", "h=%.3f t=%.3f a=%.3f ro=%.5f p=%.1f",
                h, t, a, _ro_kgm3, p_Pa);
#ifndef _DEBUG
    try
    {
#endif
        FGJSBsim* model = get_model();
        rec_atm(h, t, a, _ro_kgm3, p_Pa, wind_vx_ms, wind_vy_ms, wind_vz_ms);
        // Convert units
        double density_slugs_ft3 = ro_kgm3 * KGM3_TO_SLUGS_FT3;
        double pressure_psf = p_Pa * PA_TO_LBF_FT2;
        double soundspeed_fps = a * METERS_TO_FEET;
        double altitude_ft = h * METERS_TO_FEET;
        double temp_rankine = t * 1.8; // Kelvin to Rankine

        // Use JSBSim's built-in atmosphere override properties.
        // When these exist, FGAtmosphere::Calculate() reads them instead of
        // computing from altitude via the ISA model. Sound speed is derived
        // from the overridden temperature by JSBSim internally.
        // NOTE: Must use /fdm/jsbsim/ prefix because FGAtmosphere's PropertyManager
        // is rooted at the fdm/jsbsim subtree, not the absolute root.
        model->fgSetDouble("/fdm/jsbsim/atmosphere/override/density", density_slugs_ft3);
        model->fgSetDouble("/fdm/jsbsim/atmosphere/override/pressure", pressure_psf);
        model->fgSetDouble("/fdm/jsbsim/atmosphere/override/temperature", temp_rankine);

        // Set altitude on Propagate for LoadInputs(eAtmosphere) and ground reactions
        model->set_altitude(altitude_ft);

        // cache for debug logging
        dbg_altitude_ft = altitude_ft;
        dbg_temperature_R = temp_rankine;
        dbg_speed_of_sound_fps = soundspeed_fps;
        static double last_h;
        static bool init = false;
        double hpDiff=0;
        if (init)
        {
            hpDiff = h - last_h;
            double roc_fpm = (hpDiff / dT) * METERS_TO_FEET * 60;
            //printf("vsi: %.1f %.1f dif=%.1f -> %.1f\n", h, last_h, hpDiff, roc_fpm);
            model->fgSetDouble("/fdm/jsbsim/velocities/vsi-instant-ftmin", roc_fpm);
            last_h = h;
        }
        init = true;
        ro_kgm3 = _ro_kgm3;
#if! _DEBUG
    }
    catch (std::string ex)
    {
        MessageBoxA(0, ex.c_str(), "JSBSimEFM: Exception", 0);
        exit(0);
    } catch (const char *ex) {
        MessageBoxA(0, ex, "JSBSimEFM: Exception", 0);
        exit(0);
    }
    catch (...)
    {
        MessageBoxA(0, "Unknown failure", "JSBSimEFM: Exception", 0);
        exit(0);
    }
#endif
}

/*
called before simulation to set up your environment for the next step
*/
void ed_fm_set_current_mass_state(double mass,
    double center_of_mass_x,
    double center_of_mass_y,
    double center_of_mass_z,
    double moment_of_inertia_x,
    double moment_of_inertia_y,
    double moment_of_inertia_z)
{
   trace_callf("ed_fm_set_current_mass_state", "mass=%.3f", mass);
   rec_mass_in(mass,
               center_of_mass_x, center_of_mass_y, center_of_mass_z,
               moment_of_inertia_x, moment_of_inertia_y, moment_of_inertia_z);
   // DCS is telling us its current ground-truth mass state.  Anything we
   // previously pushed via ed_fm_change_mass is now baked into these numbers,
   // so reset the served-delta tracker.
   last_dcs_mass_kg    = mass;
   last_dcs_moi_x_kgm2 = moment_of_inertia_x;
   last_dcs_moi_y_kgm2 = moment_of_inertia_y;
   last_dcs_moi_z_kgm2 = moment_of_inertia_z;
   served_mass_delta_kg    = 0;
   served_moi_x_delta_kgm2 = 0;
   served_moi_y_delta_kgm2 = 0;
   served_moi_z_delta_kgm2 = 0;
   center_of_mass.x = center_of_mass_x;
   center_of_mass.y = center_of_mass_y;
   center_of_mass.z = center_of_mass_z;
   common_moment.x = moment_of_inertia_x;
   common_moment.y = moment_of_inertia_y;
   common_moment.z = moment_of_inertia_z;
}

/*
called before simulation to set up your environment for the next step
*/
void ed_fm_set_current_state(double ax,	//linear acceleration component in world coordinate system
    double ay,	//linear acceleration component in world coordinate system
    double az,	//linear acceleration component in world coordinate system
    double vx,	//linear velocity component in world coordinate system
    double vy,	//linear velocity component in world coordinate system
    double vz,	//linear velocity component in world coordinate system
    double px,	//center of the body position in world coordinate system
    double py,	//center of the body position in world coordinate system
    double pz,	//center of the body position in world coordinate system
    double omegadotx,	//angular accelearation components in world coordinate system
    double omegadoty,	//angular accelearation components in world coordinate system
    double omegadotz,	//angular accelearation components in world coordinate system
    double omegax,	//angular velocity components in world coordinate system
    double omegay,	//angular velocity components in world coordinate system
    double omegaz,	//angular velocity components in world coordinate system
    double quaternion_x,	//orientation quaternion components in world coordinate system
    double quaternion_y,	//orientation quaternion components in world coordinate system
    double quaternion_z,	//orientation quaternion components in world coordinate system
    double quaternion_w	//orientation quaternion components in world coordinate system
)
{
    trace_call("ed_fm_set_current_state");
    FGJSBsim *model = get_model();
    rec_state(ax, ay, az, vx, vy, vz, px, py, pz,
              omegadotx, omegadoty, omegadotz, omegax, omegay, omegaz,
              quaternion_x, quaternion_y, quaternion_z, quaternion_w);
    static JSBSim::FGQuaternion quat;
    model->set_current_state(ax, ay, az, vx, vy, vz, px, py, pz, omegadotx, omegadoty, omegadotz, omegax, omegay, omegaz, quaternion_x, quaternion_y, quaternion_z, quaternion_w);
    //quat(1) = quaternion_w;
    //quat(2) = quaternion_x;
    //quat(3) = quaternion_y;
    //quat(4) = quaternion_z;

    //JSBSim::FGColumnVector3 e = quat.GetEulerDeg();
    //printf("Q(%.3f, %.3f, %.3f, %.3f) = (ph %.1f, th %.1f, ps %.1f) \n", quaternion_x, quaternion_y, quaternion_z, quaternion_w, 
    //    e(1), e(2), e(3));

}
double Magnitude(double xv, double yv, double zv)
{
    return sqrt(xv*xv + yv*yv + zv*zv);
}

void ed_fm_set_current_state_body_axis(
    double ax, double ay, double az,                      // linear acceleration component in body coordinate system
    double vx, double vy, double vz,                      // linear velocity component in body coordinate system
    double wind_vx, double wind_vy, double wind_vz,       // wind linear velocity component in body coordinate system
    double omegadotx, double omegadoty, double omegadotz, // angular accelearation components in body coordinate system
    double omegax, double omegay, double omegaz,          // angular velocity components in body coordinate system
    double yaw, double pitch, double roll,                // radians
    double alpha_rads,                        // AoA radians
    double beta_rads	//AoS radians
)
{
    trace_call("ed_fm_set_current_state_body_axis");
    FGJSBsim *model = get_model();
    rec_state_body(ax, ay, az, vx, vy, vz, wind_vx, wind_vy, wind_vz,
                   omegadotx, omegadoty, omegadotz, omegax, omegay, omegaz,
                   yaw, pitch, roll, alpha_rads, beta_rads);
    model->set_current_state_body_axis(
        ax, ay, az,                      // linear acceleration component in body coordinate system
        vx, vy, vz,                      // linear velocity component in body coordinate system
        wind_vx, wind_vy, wind_vz,       // wind linear velocity component in body coordinate system
        omegadotx, omegadoty, omegadotz, // angular accelearation components in body coordinate system
        omegax, omegay, omegaz,          // angular velocity components in body coordinate system
        yaw, pitch, roll,                // radians
        alpha_rads,                      // AoA radians
        beta_rads,                        //AoS radians
        dT,
        ro_kgm3
    );
}

/*
Mass handling

will be called  after ed_fm_simulate :
you should collect mass changes in ed_fm_simulate

double delta_mass = 0;
double x = 0;
double y = 0;
double z = 0;
double piece_of_mass_MOI_x = 0;
double piece_of_mass_MOI_y = 0;
double piece_of_mass_MOI_z = 0;

//
while (ed_fm_change_mass(delta_mass,x,y,z,piece_of_mass_MOI_x,piece_of_mass_MOI_y,piece_of_mass_MOI_z))
{
//internal DCS calculations for changing mass, center of gravity,  and moments of inertia
}
*/
//
// ed_fm_change_mass
//
// DCS calls this in a loop after ed_fm_simulate, applying each returned delta
// until the function returns false.  We use it to keep DCS's mass / moments of
// inertia in step with JSBSim's MassBalance model.
//
// The entry.lua moment_of_inertia is the initial value DCS starts with.  Each
// frame, we compute the current target (from JSBSim's MassBalance) and report
// the incremental delta that still needs to be applied to bring DCS in line.
// DCS accumulates these deltas into its internal mass state.
//
// Mass delta is reported at delta_mass_pos = CG so DCS's parallel-axis term
// doesn't double-count the inertia delta we're also supplying explicitly.
//
// Units: DCS uses SI (kg, m, kg*m^2); JSBSim stores slugs, feet, slug*ft^2.
//
bool ed_fm_change_mass(double &delta_mass,
    double &delta_mass_pos_x,
    double &delta_mass_pos_y,
    double &delta_mass_pos_z,
    double &delta_mass_moment_of_inertia_x,
    double &delta_mass_moment_of_inertia_y,
    double &delta_mass_moment_of_inertia_z)
{
    trace_call("ed_fm_change_mass");
    FGJSBsim* model = get_model();
    if (!model) return false;

    // JSBSim current values (GetIxx/Iyy/Izz are private - use GetJ()).
    double jsb_mass_slugs = model->MassBalance->GetMass();
    const JSBSim::FGMatrix33& J = model->MassBalance->GetJ();
    double jsb_ixx_sft2   = J(1,1);
    double jsb_iyy_sft2   = J(2,2);
    double jsb_izz_sft2   = J(3,3);

    // Convert to SI
    // 1 slug = 14.593903 kg
    // 1 slug*ft^2 = 1.355817948 kg*m^2
    const double SLUG_TO_KG      = 14.59390293720636;
    const double SLUGFT2_TO_KGM2 = 1.3558179483314;

    double jsb_mass_kg    = jsb_mass_slugs * SLUG_TO_KG;
    double jsb_moi_x_kgm2 = jsb_ixx_sft2   * SLUGFT2_TO_KGM2;
    double jsb_moi_y_kgm2 = jsb_iyy_sft2   * SLUGFT2_TO_KGM2;
    double jsb_moi_z_kgm2 = jsb_izz_sft2   * SLUGFT2_TO_KGM2;

    // Compute delta relative to what DCS currently has.  DCS's current total is
    // last_dcs_* (from its set_current_mass_state) plus everything we've already
    // served via previous ed_fm_change_mass returns.
    double dcs_current_mass    = last_dcs_mass_kg     + served_mass_delta_kg;
    double dcs_current_moi_x   = last_dcs_moi_x_kgm2  + served_moi_x_delta_kgm2;
    double dcs_current_moi_y   = last_dcs_moi_y_kgm2  + served_moi_y_delta_kgm2;
    double dcs_current_moi_z   = last_dcs_moi_z_kgm2  + served_moi_z_delta_kgm2;

    double dmass  = jsb_mass_kg    - dcs_current_mass;
    double dmoi_x = jsb_moi_x_kgm2 - dcs_current_moi_x;
    double dmoi_y = jsb_moi_y_kgm2 - dcs_current_moi_y;
    double dmoi_z = jsb_moi_z_kgm2 - dcs_current_moi_z;

    // Skip if nothing meaningful has changed since last served.
    const double EPS_MASS = 0.001;      // 1 gram
    const double EPS_MOI  = 0.1;        // 0.1 kg*m^2
    if (fabs(dmass)  < EPS_MASS &&
        fabs(dmoi_x) < EPS_MOI  &&
        fabs(dmoi_y) < EPS_MOI  &&
        fabs(dmoi_z) < EPS_MOI) {
        return false;
    }

    // Mass goes in at the CG so DCS's parallel-axis term is zero and the MOI
    // delta we report is taken as-is.  CG from JSBSim is in inches (structural
    // frame) - DCS expects meters in body frame.  For a delta reported AT the
    // current CG, body-frame position is (0,0,0) in CG-relative coordinates.
    delta_mass                       = dmass;
    delta_mass_pos_x                 = 0.0;
    delta_mass_pos_y                 = 0.0;
    delta_mass_pos_z                 = 0.0;
    delta_mass_moment_of_inertia_x   = dmoi_x;
    delta_mass_moment_of_inertia_y   = dmoi_y;
    delta_mass_moment_of_inertia_z   = dmoi_z;

    // Bank what we just served so subsequent calls in the same DCS pull-loop
    // return false until the next frame produces another change.
    served_mass_delta_kg    += dmass;
    served_moi_x_delta_kgm2 += dmoi_x;
    served_moi_y_delta_kgm2 += dmoi_y;
    served_moi_z_delta_kgm2 += dmoi_z;

    rec_mass_out(dmass, 0.0, 0.0, 0.0, dmoi_x, dmoi_y, dmoi_z);

    return true;
}

/*
set internal fuel volume , init function, called on object creation and for refueling ,
you should distribute it inside at different fuel tanks
*/
void ed_fm_set_internal_fuel(double fuel)
{
    trace_callf("ed_fm_set_internal_fuel", "fuel=%.3f", fuel);
}

double ed_fm_get_internal_fuel()
{
    trace_call("ed_fm_get_internal_fuel");
    return 0;
}

/*
set external fuel volume for each payload station , called for weapon init and on reload
*/
void ed_fm_set_external_fuel(int station, double fuel, double x, double y, double z)
{
    trace_callf("ed_fm_set_external_fuel", "station=%d fuel=%.3f pos=%.3f/%.3f/%.3f",
                station, fuel, x, y, z);
}

double ed_fm_get_external_fuel()
{
    trace_call("ed_fm_get_external_fuel");
    return 0;
}

void ed_fm_configure(const char * cfg_path)
{
    trace_callf("ed_fm_configure", "path=%s", cfg_path ? cfg_path : "");
    printf("ed_fm_configure %s\n", cfg_path);
}

void ed_fm_set_plugin_data_install_path(const char* cfg_path)
{
    trace_callf("ed_fm_set_plugin_data_install_path", "path=%s", cfg_path ? cfg_path : "");
    printf("ed_fm_set_plugin_data_install_path: %s\n", cfg_path);
    root_folder = std::string(cfg_path);
}

// ============================================================================
// Gear feedback
// ----------------------------------------------------------------------------
// DCS is responsible for the suspension and the ground contact physics. 
// We provide the gear state via ed_fm_get_param it does the ground handling
// and provides the resultant force, application point, strut compression, wheel speed
//
// Weight on wheels is something that most aircraft models need and normally set
// inside FGGroundReactions but that isn't going to be called because we always
// keep the gear retracted so we will write this into gear/unit[N]/WOW and because
// this will also be written by JSBSim we are ensuring that it will be correct when
// read by the systems.
// 
// NOTE: The DCS strut index and the JSBSim gear unit index must be the same and this
//       is usually (0 nose, 1 left, 2 right) for a tricycle gear.
// ============================================================================
namespace {

struct GearStrut {
    SGPropertyNode* wow          = nullptr;   // JSBSim's own, written by us
    SGPropertyNode* compression  = nullptr;
    SGPropertyNode* wheel_speed  = nullptr;
    SGPropertyNode* integrity    = nullptr;
    SGPropertyNode* force[3]     = { nullptr, nullptr, nullptr };
    SGPropertyNode* point[3]     = { nullptr, nullptr, nullptr };
    // Drive the external_reactions forces built in FGJSBsim::init_gear_reactions:
    // the magnitude along each JSBSim body axis, and the acting point they share.
    SGPropertyNode* er_force[3]  = { nullptr, nullptr, nullptr };
    SGPropertyNode* er_point[3][3] = { { nullptr } };
};

std::map<int, GearStrut> gear_struts;

GearStrut& gear_strut(FGJSBsim* model, int idx)
{
    auto it = gear_struts.find(idx);
    if (it != gear_struts.end()) return it->second;

    GearStrut& p = gear_struts[idx];
    SGPropertyNode* root = model->PropertyManager->GetNode();
    const std::string n = std::to_string(idx);

    const std::string base = "/fdm/jsbsim/acefm/gear/unit[" + n + "]/";
    p.wow = root->getNode(base + "WOW", true);
    p.compression = root->getNode(base + "compression", true);
    p.wheel_speed = root->getNode(base + "wheel-speed-ms", true);
    p.integrity   = root->getNode(base + "integrity", true);
    static const char* axis[3] = { "x", "y", "z" };
    for (int i = 0; i < 3; i++) {
        p.force[i] = root->getNode(base + "force-" + axis[i] + "-n", true);
        p.point[i] = root->getNode(base + "force-point-" + axis[i] + "-m", true);

        // The magnitude the external force reads, in JSBSim's units and axes.
        p.er_force[i] = root->getNode(base + "force-" + std::string(axis[i]) + "-lbs", true);

        // Each of the three axis forces carries its own acting point, and all
        // three share the strut's contact point.
        const std::string er = "/fdm/jsbsim/external_reactions/acefm-gear-" + n + "-"
                             + axis[i] + "/location-";
        for (int d = 0; d < 3; d++)
            p.er_point[i][d] = root->getNode(er + axis[d] + "-in", true);
    }
    return p;
}

} // namespace

void ed_fm_suspension_feedback(int idx, const ed_fm_suspension_info * info)
{
    if (!info) {
        trace_callf("ed_fm_suspension_feedback", "idx=%d info=null", idx);
        return;
    }

    trace_callf("ed_fm_suspension_feedback",
                "idx=%d compression=%.4f integrity=%.3f wheel_speed=%.3f "
                "force=%.1f/%.1f/%.1f point=%.3f/%.3f/%.3f",
                idx, info->struct_compression, info->integrity_factor,
                info->wheel_speed_X,
                info->acting_force[0], info->acting_force[1], info->acting_force[2],
                info->acting_force_point[0], info->acting_force_point[1],
                info->acting_force_point[2]);

    FGJSBsim* model = get_model();
    if (!model || idx < 0) return;

    GearStrut& p = gear_strut(model, idx);

    p.compression->setDoubleValue(info->struct_compression);
    p.wheel_speed->setDoubleValue(info->wheel_speed_X);
    p.integrity->setDoubleValue(info->integrity_factor);
    for (int i = 0; i < 3; i++) {
        p.force[i]->setDoubleValue(info->acting_force[i]);
        p.point[i]->setDoubleValue(info->acting_force_point[i]);
    }

    // The strut carries load, so the wheel is on the ground. Either signal alone
    // is enough: a wheel can just touch with no compression yet, and a fully
    // compressed strut can momentarily read zero force as it rebounds.
    const double force_n = Magnitude(info->acting_force[0],
                                     info->acting_force[1],
                                     info->acting_force[2]);
    const bool wow = info->struct_compression > 1e-4 || force_n > 1.0;
    p.wow->setBoolValue(wow);

    // Feed the external_reactions forces, converting DCS to JSBSim.
    //   DCS body axes:     X forward, Y up,    Z right   (N, m)
    //   JSBSim body axes:  X forward, Y right, Z down    (lbs, in)
    const double fx =  info->acting_force[0] / LBS_TO_N;
    const double fy =  info->acting_force[2] / LBS_TO_N;
    const double fz = -info->acting_force[1] / LBS_TO_N;

    const double px =  info->acting_force_point[0] / INCHES_TO_METERS;
    const double py =  info->acting_force_point[2] / INCHES_TO_METERS;
    const double pz = -info->acting_force_point[1] / INCHES_TO_METERS;

    const double f[3] = { fx, fy, fz };
    const double pt[3] = { px, py, pz };
    for (int i = 0; i < 3; i++) {
        p.er_force[i]->setDoubleValue(f[i]);
        for (int d = 0; d < 3; d++)
            p.er_point[i][d]->setDoubleValue(pt[d]);
    }
}

void ed_fm_set_surface(double h,	//surface height under the center of aircraft
    double h_obj,	//surface height with objects
    unsigned surface_type, double normal_x,	//components of normal vector to surface
    double normal_y,	//components of normal vector to surface
    double normal_z	//components of normal vector to surface
)
{
    trace_callf("ed_fm_set_surface", "h=%.3f h_obj=%.3f type=%u normal=%.4f/%.4f/%.4f",
                h, h_obj, surface_type, normal_x, normal_y, normal_z);
}

/*
input handling
*/
// ============================================================================
// Trace-only entry points
// ----------------------------------------------------------------------------
// These are exported purely to discover whether DCS calls them, when, and with
// what.  Each one logs and returns the neutral value that reproduces today's
// behaviour, so exporting them changes nothing: DCS already behaves as if they
// were absent.  They become the implementation sites for the wiring work.
//
// The force and moment variants (ed_fm_add_global_force, and all four
// *_component forms) are deliberately NOT exported.  DCS may prefer a component
// path over ed_fm_add_local_force if it finds one, which would silently stop it
// taking our forces at all.  That is not a risk worth running for a trace.
// ============================================================================

void ed_fm_release()
{
    trace_call("ed_fm_release");
    trace_close();
}

void ed_fm_cold_start()
{
    trace_call("ed_fm_cold_start");
}

void ed_fm_hot_start()
{
    trace_call("ed_fm_hot_start");
}

void ed_fm_hot_start_in_air()
{
    trace_call("ed_fm_hot_start_in_air");
}

bool ed_fm_make_balance(double &ax, double &ay, double &az,
                        double &vx, double &vy, double &vz,
                        double &omegadotx, double &omegadoty, double &omegadotz,
                        double &omegax, double &omegay, double &omegaz,
                        double &yaw, double &pitch, double &roll)
{
    trace_call("ed_fm_make_balance");
    return false;   // no balance offered; DCS keeps its own spawn state
}

double ed_fm_get_shake_amplitude()
{
    trace_call("ed_fm_get_shake_amplitude");
    return 0;
}

bool ed_fm_enable_debug_info()
{
    trace_call("ed_fm_enable_debug_info");
    return false;
}

size_t ed_fm_debug_watch(int level, char *buffer, size_t maxlen)
{
    trace_callf("ed_fm_debug_watch", "level=%d maxlen=%zu", level, maxlen);
    return 0;       // nothing written, so DCS displays nothing
}

void ed_fm_on_planned_failure(const char *failure_id)
{
    trace_callf("ed_fm_on_planned_failure", "id=%s", failure_id ? failure_id : "");
}

void ed_fm_on_damage(int element, double element_integrity_factor)
{
    trace_callf("ed_fm_on_damage", "element=%d integrity=%.4f",
                element, element_integrity_factor);
}

void ed_fm_repair()
{
    trace_call("ed_fm_repair");
}

bool ed_fm_need_to_be_repaired()
{
    trace_call("ed_fm_need_to_be_repaired");
    return false;
}

void ed_fm_set_immortal(bool value)
{
    trace_callf("ed_fm_set_immortal", "value=%d", value ? 1 : 0);
}

void ed_fm_unlimited_fuel(bool value)
{
    trace_callf("ed_fm_unlimited_fuel", "value=%d", value ? 1 : 0);
}

void ed_fm_set_easy_flight(bool value)
{
    trace_callf("ed_fm_set_easy_flight", "value=%d", value ? 1 : 0);
}

// The mission-editor properties sheet.  Tracing this tells us exactly what the
// airframe's entry.lua declares and what the mission sets, which is the input
// needed to map them onto the property tree.
void ed_fm_set_property_numeric(const char *property_name, float value)
{
    trace_callf("ed_fm_set_property_numeric", "name=%s value=%.6g",
                property_name ? property_name : "", (double)value);
}

void ed_fm_set_property_string(const char *property_name, const char *value)
{
    trace_callf("ed_fm_set_property_string", "name=%s value=%s",
                property_name ? property_name : "", value ? value : "");
}

bool ed_fm_pop_simulation_event(ed_fm_simulation_event &out)
{
    trace_call("ed_fm_pop_simulation_event");
    return false;   // no events to report; DCS stops asking this frame
}

bool ed_fm_push_simulation_event(const ed_fm_simulation_event &in)
{
    trace_callf("ed_fm_push_simulation_event", "type=%u msg=%s p0=%.4f p1=%.4f",
                in.event_type, in.event_message, in.event_params[0],
                in.event_params[1]);
    return false;
}

void ed_fm_refueling_add_fuel(double fuel)
{
    trace_callf("ed_fm_refueling_add_fuel", "fuel=%.3f", fuel);
}

bool ed_fm_LERX_vortex_update(unsigned idx, LERX_vortex &out)
{
    trace_callf("ed_fm_LERX_vortex_update", "idx=%u", idx);
    return false;   // no vortices; DCS stops iterating
}

void ed_fm_wind_vector_field_update_request(wind_vector_field &in_out)
{
    trace_call("ed_fm_wind_vector_field_update_request");
    in_out.field = nullptr;         // no field requested; DCS fills nothing
    in_out.field_points_count = 0;
}

void ed_fm_wind_vector_field_done()
{
    trace_call("ed_fm_wind_vector_field_done");
}


void ed_fm_set_command(int command, float value)
{
    trace_callf("ed_fm_set_command", "command=%d value=%.6g", command, (double)value);
    rec_cmd(command, value);

    // All input bindings are config-driven: aceFMconfig.xml <sim><commands>
    // maps each DCS iCommand code to a JSBSim property via a scale/offset/clip
    // transform or a fixed value.  See the README ("Commands") for the format
    // and the bindings needed for throttle, flight controls and gear.
    DCS_interface* dcs = get_dcs_interface();
    if (dcs)
        dcs->commands.Handle(command, value);

    if (command < iCommandKeyNull)
        printf("Command %d -> %.2f\n", command, value);
}

/* List of indepdendent variables in the aero model
aero/alpha-deg
aero/alphadot-rad_sec
aero/beta-deg
aero/betadot-rad_sec
aero/bi2vel
aero/ci2vel
aero/pb
aero/qb
aero/rb
fcs/aileron-pos-deg
fcs/elevator-pos-deg
fcs/flap-pos-deg
fcs/rudder-pos-deg
fcs/slat-pos-deg
fcs/speedbrake-pos-norm
gear/gear-pos-norm
mass/tip-tanks-fitted
velocities/mach
aero/qbar-psf
metrics/Sw-sqft
metrics/bw-ft
metrics/cbarw-ft
metrics/bw-ft
velocities/vt-fps

*/

