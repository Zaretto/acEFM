#include <stdafx.h>
#include <cstdlib>    //    size_t
#include <stdlib.h>
#include <string>
#include <sstream>
#include <iomanip>
#include <vector>

#include "JSBSim_interface.h"
#include "FGFDMExec.h"
#include "FGJSBBase.h"
#include "initialization/FGInitialCondition.h"
#include "initialization/FGTrim.h"
#include "models/FGModel.h"
#include "models/FGAircraft.h"
#include "models/FGFCS.h"
#include "models/FGPropagate.h"
#include "models/FGAuxiliary.h"
#include "models/FGInertial.h"
#include "models/FGAtmosphere.h"
#include "models/FGMassBalance.h"
#include "models/FGAerodynamics.h"
#include "models/FGLGear.h"
#include "models/FGGroundReactions.h"
#include "models/FGPropulsion.h"
#include "models/FGAccelerations.h"
#include "models/FGInput.h"
#include "models/atmosphere/FGWinds.h"
#include "models/propulsion/FGEngine.h"
#include "models/propulsion/FGPiston.h"
#include "models/propulsion/FGTurbine.h"
#include "models/propulsion/FGTurboProp.h"
#include "models/propulsion/FGRocket.h"
#include "models/propulsion/FGElectric.h"
#include "models/propulsion/FGNozzle.h"
#include "models/propulsion/FGPropeller.h"
#include "models/propulsion/FGRotor.h"
#include "models/propulsion/FGTank.h"
#include "input_output/FGPropertyManager.h"
#include "input_output/FGGroundCallback.h"
#include <input_output/FGXMLFileRead.h>

#include "PropertyVisitor.h"

using namespace JSBSim;

#if defined(ACEFM_PROFILE)
#include <chrono>
// JSBSim step-throughput profiler (compile-time opt-in: define ACEFM_PROFILE).
// Accumulates the wall-clock cost of each fdmex->Run() and reports
// microseconds-per-step and the implied maximum rate once per second.
// Originally lived in JSBSim (print_time / REPORT_FRAME_RATE); moved here so
// JSBSim stays clean. steady_clock is monotonic and portable.
static void acefm_profile_step(std::chrono::steady_clock::duration step)
{
    using namespace std::chrono;
    static steady_clock::time_point window = steady_clock::now();
    static long long ns = 0;
    static unsigned long frames = 0;
    ns += duration_cast<nanoseconds>(step).count();
    ++frames;
    if (steady_clock::now() - window >= seconds(1) && frames) {
        double us = (ns / 1000.0) / frames;
        printf("[acEFM] JSBSim step: %.2f us/step  (%.0f Hz max)  over %lu frames\n",
               us, 1.0e6 / us, frames);
        window = steady_clock::now();
        ns = 0;
        frames = 0;
    }
}
#endif

static inline double
FMAX(double a, double b)
{
    return a > b ? a : b;
}
std::string root_folder;

FGJSBsim::FGJSBsim(double dt)
{
    bool result;

    PropertyManager = new FGPropertyManager();
    fdmex = new FGFDMExec(PropertyManager);

    Atmosphere = fdmex->GetAtmosphere();
    Winds = fdmex->GetWinds();
    FCS = fdmex->GetFCS();
    MassBalance = fdmex->GetMassBalance();
    Propulsion = fdmex->GetPropulsion();
    Aircraft = fdmex->GetAircraft();
    Propagate = fdmex->GetPropagate();
    Auxiliary = fdmex->GetAuxiliary();

    //Inertial = fdmex->GetInertial();
    Aerodynamics = fdmex->GetAerodynamics();
    GroundReactions = fdmex->GetGroundReactions();
    //Accelerations = fdmex->GetAccelerations();

    fgic = fdmex->GetIC();
    needTrim = true;

    // deprecate sim-time-sec for simulation/sim-time-sec
    // remove alias with increased configuration file version number (2.1 or later)
    SGPropertyNode * node = PropertyManager->GetNode("/fdm/jsbsim/simulation/sim-time-sec");
    PropertyManager->GetNode("/fdm/jsbsim/sim-time-sec", true)->alias(node);
    // end of sim-time-sec deprecation patch

    for (unsigned int i = 0; i < Propulsion->GetNumEngines(); i++) {
        SGPropertyNode * node = PropertyManager->GetNode("engines/engine", i, true);
        Propulsion->GetEngine(i)->GetThruster()->SetRPM(node->getDoubleValue("rpm") /
            Propulsion->GetEngine(i)->GetThruster()->GetGearRatio());
    }
    if (dt)
        fdmex->Setdt(dt);
    else
        fdmex->Setdt(0.006);
    
    std::string aircraft_model("efm-aero");
    auto aircraft_path = SGPath(root_folder + ("/EFM"));
    auto engine_path = SGPath(root_folder + ("/EFM/engines"));
    auto systems_path = SGPath(root_folder + ("/EFM/systems"));

    FGXMLFileRead XMLFileRead;
    auto config_file = SGPath(root_folder + "\\acEFMconfig.xml");
    
    SG_LOG(SG_FLIGHT, SG_INFO, "Config from " << config_file);

    PropsVisitor v(PropertyManager->GetNode(), "");
    Element* document = XMLFileRead.LoadXMLDocument(config_file, v);
    SGPropertyNode* aeroNode = PropertyManager->GetNode(std::string("/sim/aero"));
    if (aeroNode == nullptr)
        throw "Missing /sim/aero in config file";
    aircraft_model = aeroNode->getStringValue();
    
    auto debugNode = PropertyManager->GetNode("/fdm/jsbsim/acefm/debug-level");
    if (debugNode != nullptr)
        fdmex->debug_lvl = debugNode->getIntValue();

    initDebugNodes();
    result = fdmex->LoadModel(aircraft_path, engine_path, systems_path, aircraft_model, false);
    Wingspan = fgGetDouble("/fdm/jsbsim/metrics/bw-ft");
    Wingchord = fgGetDouble("/fdm/jsbsim/metrics/cbarw-ft"); 

    if (result) {
        SG_LOG(SG_FLIGHT, SG_INFO, "  loaded aero.");
    }
    else {
        SG_LOG(SG_FLIGHT, SG_INFO,
            "  aero does not exist (you may have mis-typed the name).");
        throw(-1);
    }

    SG_LOG(SG_FLIGHT, SG_INFO, "");
    SG_LOG(SG_FLIGHT, SG_INFO, "");
    SG_LOG(SG_FLIGHT, SG_INFO, "After loading aero definition file ...");

    int Neng = Propulsion->GetNumEngines();
    SG_LOG(SG_FLIGHT, SG_INFO, "num engines = " << Neng);

    if (GroundReactions->GetNumGearUnits() <= 0) {
        SG_LOG(SG_FLIGHT, SG_ALERT, "num gear units = "
            << GroundReactions->GetNumGearUnits());
        SG_LOG(SG_FLIGHT, SG_ALERT, "This is a very bad thing because with 0 gear units, the ground trimming");
        SG_LOG(SG_FLIGHT, SG_ALERT, "routine (coming up later in the code) will core dump.");
        SG_LOG(SG_FLIGHT, SG_ALERT, "Halting the sim now, and hoping a solution will present itself soon!");
        exit(-1);
    }

    init_gear();

    Propulsion->SetFuelFreeze((PropertyManager->GetNode("/sim/freeze/fuel", true))->getBoolValue());

    fgSetDouble("/fdm/trim/pitch-trim", FCS->GetPitchTrimCmd());
    fgSetDouble("/fdm/trim/throttle", FCS->GetThrottleCmd(0));
    fgSetDouble("/fdm/trim/aileron", FCS->GetDaCmd());
    fgSetDouble("/fdm/trim/rudder", FCS->GetDrCmd());

    OpenPropertyInspectionPort();

    crashed = false;
}

/******************************************************************************/

void FGJSBsim::initDebugNodes()
{
    auto get = [this](const char* path) {
        return PropertyManager->GetNode(path, true);
    };

    dbg.flag            = get("/fdm/jsbsim/acefm/debug");
    // timing
    dbg.dt_dcs          = get("/fdm/jsbsim/acefm/debug/dt-dcs");
    dbg.wallclock_us    = get("/fdm/jsbsim/acefm/debug/wallclock-us");
    dbg.wall_dt_us      = get("/fdm/jsbsim/acefm/debug/wall-dt-us");
    dbg.frame_count     = get("/fdm/jsbsim/acefm/debug/frame-count");
    // forces
    dbg.fbx             = get("/fdm/jsbsim/acefm/debug/forces-fbx-lbs");
    dbg.fby             = get("/fdm/jsbsim/acefm/debug/forces-fby-lbs");
    dbg.fbz             = get("/fdm/jsbsim/acefm/debug/forces-fbz-lbs");
    // moments
    dbg.ml              = get("/fdm/jsbsim/acefm/debug/moments-l-lbsft");
    dbg.mm              = get("/fdm/jsbsim/acefm/debug/moments-m-lbsft");
    dbg.mn              = get("/fdm/jsbsim/acefm/debug/moments-n-lbsft");
    // engines
    dbg.num_engines     = get("/fdm/jsbsim/acefm/debug/num-engines");
    for (int e = 0; e < MAX_ENGINES; e++) {
        std::string es = std::to_string(e);
        dbg.throttle[e]     = get(("/fdm/jsbsim/acefm/debug/throttle-cmd-" + es).c_str());
        dbg.thrust[e]       = get(("/fdm/jsbsim/acefm/debug/thrust-lbs-" + es).c_str());
        dbg.n1[e]           = get(("/fdm/jsbsim/acefm/debug/n1-" + es).c_str());
        dbg.src_throttle[e] = get(("/fdm/jsbsim/fcs/throttle-cmd-norm[" + es + "]").c_str());
        dbg.src_thrust[e]   = get(("/fdm/jsbsim/propulsion/engine[" + es + "]/thrust-lbs").c_str());
        dbg.src_n1[e]       = get(("/fdm/jsbsim/propulsion/engine[" + es + "]/n1").c_str());
    }
    // atmosphere
    dbg.density_slugft3 = get("/fdm/jsbsim/acefm/debug/density-slugft3");
    dbg.pressure_lbfft2 = get("/fdm/jsbsim/acefm/debug/pressure-lbfft2");
    dbg.altitude_ft     = get("/fdm/jsbsim/acefm/debug/altitude-ft");
    dbg.temperature_R   = get("/fdm/jsbsim/acefm/debug/temperature-R");
    dbg.speed_of_sound_fps = get("/fdm/jsbsim/acefm/debug/speed-of-sound-fps");
    // aero state
    dbg.alpha_deg       = get("/fdm/jsbsim/acefm/debug/alpha-deg");
    dbg.beta_deg        = get("/fdm/jsbsim/acefm/debug/beta-deg");
    dbg.alphadot        = get("/fdm/jsbsim/acefm/debug/alphadot-radsec");
    dbg.betadot         = get("/fdm/jsbsim/acefm/debug/betadot-radsec");
    dbg.qbar            = get("/fdm/jsbsim/acefm/debug/qbar-psf");
    dbg.vt_fps          = get("/fdm/jsbsim/acefm/debug/vt-fps");
    dbg.vt_ms           = get("/fdm/jsbsim/acefm/debug/vt-ms");
    dbg.vc_kts          = get("/fdm/jsbsim/acefm/debug/vc-kts");
    dbg.bi2vel          = get("/fdm/jsbsim/acefm/debug/bi2vel");
    dbg.ci2vel          = get("/fdm/jsbsim/acefm/debug/ci2vel");
    dbg.p               = get("/fdm/jsbsim/acefm/debug/p-radsec");
    dbg.q               = get("/fdm/jsbsim/acefm/debug/q-radsec");
    dbg.r               = get("/fdm/jsbsim/acefm/debug/r-radsec");
    dbg.density_kgm3    = get("/fdm/jsbsim/acefm/debug/density-kgm3");
    dbg.pitch           = get("/fdm/jsbsim/acefm/debug/pitch-rad");
    dbg.roll            = get("/fdm/jsbsim/acefm/debug/roll-rad");
    dbg.yaw             = get("/fdm/jsbsim/acefm/debug/yaw-rad");
    // source nodes
    dbg.src_fbx         = get("/fdm/jsbsim/forces/fbx-total-lbs");
    dbg.src_fby         = get("/fdm/jsbsim/forces/fby-total-lbs");
    dbg.src_fbz         = get("/fdm/jsbsim/forces/fbz-total-lbs");
    dbg.src_ml          = get("/fdm/jsbsim/moments/l-total-lbsft");
    dbg.src_mm          = get("/fdm/jsbsim/moments/m-total-lbsft");
    dbg.src_mn          = get("/fdm/jsbsim/moments/n-total-lbsft");
    dbg.src_adot        = get("/fdm/jsbsim/aero/alphadot-rad_sec");
    dbg.src_bdot        = get("/fdm/jsbsim/aero/betadot-rad_sec");
}

/******************************************************************************/
FGJSBsim::~FGJSBsim(void)
{
    delete fdmex;
    delete PropertyManager;
}

/******************************************************************************/
double get_Altitude()
{
    return 1000.0;
}
// Initialize the JSBsim flight model, dt is the time increment for
// each subsequent iteration through the EOM

void FGJSBsim::init()
{
    SG_LOG(SG_FLIGHT, SG_INFO, "Starting and initializing JSBsim");

    fgic->SetVcalibratedKtsIC(400);
    fgic->SetAltitudeAGLFtIC(10000);
    fgic->SetClimbRateFpmIC(0);

    /*   copy_to_JSBsim();*/
    fgSetDouble("/environment/dewpoint-degc", 10);
    fgSetDouble("/environment/relative-humidity", 15);
    fgSetDouble("/position/altitude-ft", 10000);
    //Propagate->SetAltitudeASL(altitude->getDoubleValue());
    fdmex->RunIC(); //loop JSBSim once w/o integrating
    if (fgGetBool("/sim/presets/running")) {
        // Initialize engines at idle, not full power.
        // InitRunning() forces throttle=1.0 and converges at max thrust,
        // leaving N1=100%. Instead we set throttle to idle first, then let
        // InitRunning set the running state, and GetSteadyState converges
        // at idle thrust.
        for (unsigned int i = 0; i < Propulsion->GetNumEngines(); i++) {
            FCS->SetThrottleCmd(i, 0.0);
            FCS->SetThrottlePos(i, 0.0);
            Propulsion->GetEngine(i)->InitRunning();
        }
    }

    // DCS owns the state, so set all four Propagate integrators to eNone which
    // disables integration and permits injection of the state injected in
    // set_current_state_body_axis
    fgSetDouble("/fdm/jsbsim/simulation/integrator/rate/rotational", 0);
    fgSetDouble("/fdm/jsbsim/simulation/integrator/rate/translational", 0);
    fgSetDouble("/fdm/jsbsim/simulation/integrator/position/rotational", 0);
    fgSetDouble("/fdm/jsbsim/simulation/integrator/position/translational", 0);

    // DCS also owns the wind
    Winds->SetTurbType(JSBSim::FGWinds::ttNone);


    SG_LOG(SG_FLIGHT, SG_INFO, "  Initialized JSBSim with:");
}

/******************************************************************************/

void FGJSBsim::unbind()
{
    fdmex->Unbind();
}

/******************************************************************************/

// Update the jsbsim model (the DCS-owned state has been set before this call and
// will not be integrated over). 
void FGJSBsim::update(double dt)
{
    if (crashed) {
        if (!fgGetBool("/sim/crashed"))
            fgSetBool("/sim/crashed", true);
        return;
    }
    fdmex->Setdt(dt);

    // DCSW World's world is static and does not rotate so so force to zero each frame
    // otherwise lon would advance towards the west at earth rate.
    Propagate->SetEarthPositionAngle(0.0);

#if defined(ACEFM_PROFILE)
    auto _run_t0 = std::chrono::steady_clock::now();
#endif
    bool success = fdmex->Run();
#if defined(ACEFM_PROFILE)
    acefm_profile_step(std::chrono::steady_clock::now() - _run_t0);
#endif

    if (!success) {
        crashed = true;
        return;
    }

    update_external_forces(fdmex->GetSimTime());
}

/******************************************************************************/

void FGJSBsim::suspend()
{
    fdmex->Hold();
}

/******************************************************************************/

void FGJSBsim::resume()
{
    fdmex->Resume();
}

/******************************************************************************/


bool FGJSBsim::ToggleDataLogging(void)
{
    // ToDo: handle this properly
    fdmex->DisableOutput();
    return false;
}


bool FGJSBsim::ToggleDataLogging(bool state)
{
    if (state) {
        fdmex->EnableOutput();
        return true;
    }
    else {
        fdmex->DisableOutput();
        return false;
    }
}


void FGJSBsim::init_gear(void)
{
    int Ngear = GroundReactions->GetNumGearUnits();
    for (int i = 0; i<Ngear; i++) {
        auto gear = GroundReactions->GetGearUnit(i);
        SGPropertyNode * node = PropertyManager->GetNode("gear/gear", i, true);
        node->setDoubleValue("xoffset-in", gear->GetBodyLocation()(1) * 12);
        node->setDoubleValue("yoffset-in", gear->GetBodyLocation()(2) * 12);
        node->setDoubleValue("zoffset-in", gear->GetBodyLocation()(3) * 12);
        node->setBoolValue("wow", gear->GetWOW());
        node->setDoubleValue("rollspeed-ms", gear->GetWheelRollVel()*0.3043);
        node->setBoolValue("has-brake", gear->GetBrakeGroup() > 0);
        node->setDoubleValue("position-norm", gear->GetGearUnitPos());
        //    node->setDoubleValue("tire-pressure-norm", gear->GetTirePressure());
        node->setDoubleValue("compression-norm", gear->GetCompLen());
        node->setDoubleValue("compression-ft", gear->GetCompLen());
        if (gear->GetSteerable())
            node->setDoubleValue("steering-norm", gear->GetSteerNorm());
    }
}

void FGJSBsim::update_gear(void)
{
    int Ngear = GroundReactions->GetNumGearUnits();
    for (int i = 0; i<Ngear; i++) {
        auto gear = GroundReactions->GetGearUnit(i);
        SGPropertyNode * node = PropertyManager->GetNode("gear/gear", i, true);
        node->getChild("wow", 0, true)->setBoolValue(gear->GetWOW());
        node->getChild("rollspeed-ms", 0, true)->setDoubleValue(gear->GetWheelRollVel()*0.3043);
        node->getChild("position-norm", 0, true)->setDoubleValue(gear->GetGearUnitPos());
        //    gear->SetTirePressure(node->getDoubleValue("tire-pressure-norm"));
        node->setDoubleValue("compression-norm", gear->GetCompLen());
        node->setDoubleValue("compression-ft", gear->GetCompLen());
        if (gear->GetSteerable())
            node->setDoubleValue("steering-norm", gear->GetSteerNorm());
    }
}

inline static double sqr(double x)
{
    return x * x;
}

static double angle_diff(double a, double b)
{
    double diff = fabs(a - b);
    if (diff > 180) diff = 360 - diff;

    return diff;
}

static void check_hook_solution(const FGColumnVector3& ground_normal_body, double E, double hook_length, double sin_fi_guess, double cos_fi_guess, double* sin_fis, double* cos_fis, double* fis, int* points)
{
    FGColumnVector3 tip(-hook_length * cos_fi_guess, 0, hook_length * sin_fi_guess);
    double dist = DotProduct(tip, ground_normal_body);
    if (fabs(dist + E) < 0.0001) {
        sin_fis[*points] = sin_fi_guess;
        cos_fis[*points] = cos_fi_guess;
        fis[*points] = atan2(sin_fi_guess, cos_fi_guess) * RADIANS_TO_DEGREES;
        (*points)++;
    }
}


static void check_hook_solution(const FGColumnVector3& ground_normal_body, double E, double hook_length, double sin_fi_guess, double* sin_fis, double* cos_fis, double* fis, int* points)
{
    if (sin_fi_guess >= -1 && sin_fi_guess <= 1) {
        double cos_fi_guess = sqrt(1 - sqr(sin_fi_guess));
        check_hook_solution(ground_normal_body, E, hook_length, sin_fi_guess, cos_fi_guess, sin_fis, cos_fis, fis, points);
        if (fabs(cos_fi_guess) > JSB_NEARLY_ZERO) {
            check_hook_solution(ground_normal_body, E, hook_length, sin_fi_guess, -cos_fi_guess, sin_fis, cos_fis, fis, points);
        }
    }
}

void FGJSBsim::update_external_forces(double t_off)
{
    // not relevant because DCS world owns all of this.
}
void FGJSBsim::set_roll_pitch_yaw(double roll_rad, double pitch_rad, double yaw_rad)
{
    // Euler angles -> local-to-body quaternion (phi, theta, psi).
    JSBSim::FGQuaternion qLocal(roll_rad, pitch_rad, yaw_rad);

    // Convert to ECI via the current local frame, mirroring FGPropagate's IC
    // (FGPropagate.cpp: qAttitudeECI = Ti2l.GetQuaternion() * qAttitudeLocal).
    // SetInertialOrientation rebuilds Tl2b/Tb2l and qAttitudeLocal coherently;
    // setting qAttitudeLocal alone via SetVState does not stick (it is derived
    // from qAttitudeECI there).
    Propagate->SetInertialOrientation(Propagate->GetTi2l().GetQuaternion() * qLocal);
}

void FGJSBsim::update()
{
    // nothing to do
}

void FGJSBsim::set_fcs_elevator_cmd_norm(double v)
{
    static double ltv = -1000000;
    if (fabs(v - ltv) > 0.001) {
        //printf("Elevator %.2f\n", v);
        ltv = v;
        fgSetDouble("/fdm/jsbsim/fcs/elevator-cmd-norm", v);
    }
}

void FGJSBsim::set_fcs_aileron_cmd_norm(double v)
{
    static double ltv = -1000000;
    if (fabs(v - ltv) > 0.001) {
        //printf("Aileron %.2f\n", v);
        ltv = v;
        fgSetDouble("/fdm/jsbsim/fcs/aileron-cmd-norm", v);
    }
}
void FGJSBsim::set_fcs_rudder_cmd_norm(double v)
{
    static double ltv = -1000000;
    if (fabs(v - ltv) > 0.001) {
        //printf("Rudder %.2f\n", v);
        ltv = v;
        fgSetDouble("/fdm/jsbsim/fcs/rudder-cmd-norm", v);
    }
}
double FGJSBsim::get_pressure_at_atltiude_lbf_ft2(double altitude) const
{
    return Atmosphere->GetPressure(altitude);
}
void FGJSBsim::set_fcs_gear_cmd_norm(double v)
{
    fgSetDouble("/fdm/jsbsim/gear/gear-cmd-norm", v);
}
void FGJSBsim::set_fcs_throttle_cmd_norm(int e, double v)
{
    static double ltv[5] = {
        -1000000,
        -1000000,
        -1000000,
        -1000000,
        -1000000,
    };
    if (fabs(v - ltv[e]) > 0.001) {
        printf("Throttle %d %.2f\n",e, v);
        ltv[e] = v;
        if (e == 0)
            fgSetDouble("/fdm/jsbsim/fcs/throttle-cmd-norm[0]", v);
        else if (e == 1)
            fgSetDouble("/fdm/jsbsim/fcs/throttle-cmd-norm[1]", v);
        else if (e == 2)
            fgSetDouble("/fdm/jsbsim/fcs/throttle-cmd-norm[2]", v);
        else if (e == 3)
            fgSetDouble("/fdm/jsbsim/fcs/throttle-cmd-norm[3]", v);
        else if (e == 4)
            fgSetDouble("/fdm/jsbsim/fcs/throttle-cmd-norm[4]", v);
    }
}
double FGJSBsim::get_atmosphere_rho_slugs_ft3()
{
    return Atmosphere->GetDensity();
}
double FGJSBsim::get_atmosphere_pressure_lbf_ft2()
{
    return Atmosphere->GetPressure();
}
const JSBSim::FGColumnVector3& FGJSBsim::GetXYZcg(void) {
    return MassBalance->GetXYZcg();
}
const JSBSim::FGColumnVector3& FGJSBsim::GetXYZrp(void) {
    return Aircraft->GetXYZrp();
}

void FGJSBsim::set_altitude(double h_ft)
{
    Propagate->SetAltitudeASL(h_ft);
}

void FGJSBsim::set_current_state(double ax,	//linear acceleration component in world coordinate system
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
    // Nothing to inject from the world-axis state. The load factors
    // (accelerations/Nx|Ny|Nz) are computed by Auxiliary from JSBSim's own
    // force total; the writes that used to sit here targeted read-only tied
    // properties and had no effect.
}

static double Magnitude(double xv, double yv, double zv)
{
    return sqrt(xv * xv + yv * yv + zv * zv);
}

void FGJSBsim::set_current_state_body_axis(
    double ax, double ay, double az,                      // linear acceleration component in body coordinate system
    double vx, double vy, double vz,                      // linear velocity component in body coordinate system
    double wind_vx, double wind_vy, double wind_vz,       // wind linear velocity component in body coordinate system
    double omegadotx, double omegadoty, double omegadotz, // angular accelearation components in body coordinate system
    double omegax, double omegay, double omegaz,          // angular velocity components in body coordinate system
    double yaw, double pitch, double roll,                // radians
    double alpha_rads,                                    // AoA radians
    double beta_rads,                                      //AoS radians
    double dT,
    double ro_kgm3
)
{
    // Attitude first. SetUVW converts body velocity to inertial velocity using
    // the transforms in force at the time of the call, and Propagate::Run's
    // CalculateUVW() inverts that conversion using the transforms current at Run
    // time. Setting the attitude before the velocity makes both directions use
    // the same frame; the old velocity-first order rotated the recovered body
    // velocity by one frame's attitude change.
    // This sets qAttitudeLocal and the body transforms; the attitude/*
    // properties are tied to Propagate getters and report from it, so no manual
    // property writes are needed here.
    set_roll_pitch_yaw(roll, pitch, yaw);

    // Inject ground-relative body velocity into Propagate. FGWinds runs as a
    // normal model: the DCS wind is fed to it as NED below, and Auxiliary::Run()
    // then computes vAeroUVW = in.vUVW - Tl2b*windNED.
    // DCS axes: X forward, Y up, Z right  →  JSBSim: X forward, Y right, Z down
    Propagate->SetUVW(1, vx * METER_TO_FEET_FACTOR);
    Propagate->SetUVW(2, vz * METER_TO_FEET_FACTOR);
    Propagate->SetUVW(3, -vy * METER_TO_FEET_FACTOR);

    // Angular rates in JSBSim body frame
    // DCS: omegax=roll, omegay=yaw(Y+up), omegaz=pitch
    // JSBSim: P=roll(X), Q=pitch(Y), R=yaw(Z+down)
    Propagate->SetPQR(1, omegax);
    Propagate->SetPQR(2, omegaz);
    Propagate->SetPQR(3, -omegay);

    // Feed DCS wind (body axes) to FGWinds as NED, using the attitude just set.
    // FGWinds::Run() publishes it as TotalWindNED (turbulence disabled at init),
    // and Auxiliary subtracts it from the ground-relative velocity to recover the
    // aero-relative velocity:  windNED = Tb2l * windBody.
    JSBSim::FGColumnVector3 windBody( wind_vx * METER_TO_FEET_FACTOR,
                                      wind_vz * METER_TO_FEET_FACTOR,
                                     -wind_vy * METER_TO_FEET_FACTOR );
    Winds->SetWindNED(Propagate->GetTb2l() * windBody);

    // --- debug instrumentation: DCS-provided aero state for comparison ---
    if (dbg.flag->getBoolValue()) {
        // DCS-provided Vt for debug logging (Auxiliary will compute its own from vAeroUVW)
        double Vt_ms = Magnitude(vx - wind_vx, vy - wind_vy, vz - wind_vz);
        double Vt_fps = Vt_ms * METER_TO_FEET_FACTOR;
        double VcKts = Vt_fps / 1.68781;
        double twovel = Vt_fps * 2;
        double qbar_psf = 0.5 * (ro_kgm3 * KGM3_TO_SLUGS_FT3) * Vt_fps * Vt_fps;
        double bi2vel = (twovel > JSB_NEARLY_ZERO) ? Wingspan / twovel : 0;
        double ci2vel = (twovel > JSB_NEARLY_ZERO) ? Wingchord / twovel : 0;
        dbg.alpha_deg->setDoubleValue(alpha_rads * RADIANS_TO_DEGREES);
        dbg.beta_deg->setDoubleValue(beta_rads * RADIANS_TO_DEGREES);
        dbg.alphadot->setDoubleValue(dbg.src_adot->getDoubleValue());
        dbg.betadot->setDoubleValue(dbg.src_bdot->getDoubleValue());
        dbg.qbar->setDoubleValue(qbar_psf);
        dbg.vt_fps->setDoubleValue(Vt_fps);
        dbg.vt_ms->setDoubleValue(Vt_ms);
        dbg.vc_kts->setDoubleValue(VcKts);
        dbg.bi2vel->setDoubleValue(bi2vel);
        dbg.ci2vel->setDoubleValue(ci2vel);
        dbg.p->setDoubleValue(omegax);
        dbg.q->setDoubleValue(omegaz);
        dbg.r->setDoubleValue(-omegay);
        dbg.density_kgm3->setDoubleValue(ro_kgm3);
        dbg.pitch->setDoubleValue(pitch);
        dbg.roll->setDoubleValue(roll);
        dbg.yaw->setDoubleValue(yaw);
    }
}
void FGJSBsim::OpenPropertyInspectionPort(int port)
{
    auto input = fdmex->GetInput();

    // Create the Element via Element_ptr (SGSharedPtr<Element>) so that
    // reference counting is correct. FGInput::Load wraps the raw pointer in
    // SGSharedPtr internally; using Element_ptr here ensures the element
    // survives until all internal references are released.
    JSBSim::Element_ptr inputElement(new JSBSim::Element("input"));
    inputElement->AddAttribute("port", std::to_string(port));

    input->Load(inputElement);
    input->Enable();
}
