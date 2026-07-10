#ifndef _JSBSIM_HXX
#define _JSBSIM_HXX

#include <simgear/props/props.hxx>

#include "FGFDMExec.h"

#undef MAX_ENGINES
#define MAX_ENGINES 4
#include "math/FGColumnVector3.h"

#define ID_JSBSIMXX "$Header JSBSim.hxx,v 1.4 2000/10/22 14:02:16 jsb Exp $"

#define METERS_TO_FEET 3.2808398950
#define RADTODEG 57.2957795

#define METER_TO_FEET_FACTOR 3.28084
#define FEET_TO_METER_FACTOR   0.3048
#define FEET2_TO_METER2_FACTOR 10.76391
#define DEGREES_TO_RADIANS 0.0174532925
#define RADIANS_TO_DEGREES 57.295779505
#define JSB_NEARLY_ZERO 0.0000001 // avoid division by zero
#define LBSFT_TO_NM 1.3558179483314
#define LBS_TO_N 4.4483985765124555160142348754448
#define KGM3_TO_SLUGS_FT3 0.0019403203
#define INCHES_TO_METERS 0.0254
#define SLUG_FT2_TO_KG_M2 1.35594
#define PA_TO_LBF_FT2 0.02088527017024426350851221317543

extern std::string root_folder;

namespace JSBSim {
    class FGAtmosphere;
    class FGWinds;
    class FGFCS;
    class FGPropulsion;
    class FGMassBalance;
    class FGAerodynamics;
    class FGInertial;
    class FGAircraft;
    class FGPropagate;
    class FGAuxiliary;
    class FGOutput;
    class FGInitialCondition;
    class FGLocation;
    class FGAccelerations;
    class FGPropertyManager;
    class FGColumnVector3;
}

class FGJSBsim
{
public:
    /// Constructor
    FGJSBsim(double dt);
    /// Destructor
    ~FGJSBsim();

    void init();
    void unbind();
    void suspend();
    void resume();
    void update(double dt);

    bool ToggleDataLogging(bool state);
    bool ToggleDataLogging(void);
    void OpenPropertyInspectionPort(int port = 1137);

    double fgGetDouble(const char* name, double defaultValue = 0.0)
    {
        return PropertyManager->GetNode()->getDoubleValue(name, defaultValue);
    }
    double fgSetDouble(const char* name, double value)
    {
        return PropertyManager->GetNode()->setDoubleValue(name, value);
    }
    double fgGetBool(const char* name, bool defaultValue = false)
    {
        return PropertyManager->GetNode()->getBoolValue(name, defaultValue);
    }
    double fgSetBool(const char* name, bool value)
    {
        return PropertyManager->GetNode()->setBoolValue(name, value);
    }
    bool caught_wire_m(double t, const double pt[4][3])
    {
        return false;
    }
    void set_roll_pitch_yaw(double roll, double pitch, double yaw);
    void update(); // no-op in DCS mode; kept for interface compat
    void set_fcs_elevator_cmd_norm(double v);
    void set_fcs_aileron_cmd_norm(double v);
    void set_fcs_rudder_cmd_norm(double v);
    void set_fcs_throttle_cmd_norm(int e, double v);
    void set_fcs_gear_cmd_norm(double v);
    double get_pressure_at_atltiude_lbf_ft2(double altitude) const;
    double get_atmosphere_rho_slugs_ft3();
    double get_atmosphere_pressure_lbf_ft2();
    const JSBSim::FGColumnVector3& GetXYZcg(void);
    const JSBSim::FGColumnVector3& GetXYZrp(void);
    void set_altitude(double h_ft);
    void set_current_state(double ax,           //linear acceleration component in world coordinate system
                           double ay,           //linear acceleration component in world coordinate system
                           double az,           //linear acceleration component in world coordinate system
                           double vx,           //linear velocity component in world coordinate system
                           double vy,           //linear velocity component in world coordinate system
                           double vz,           //linear velocity component in world coordinate system
                           double px,           //center of the body position in world coordinate system
                           double py,           //center of the body position in world coordinate system
                           double pz,           //center of the body position in world coordinate system
                           double omegadotx,    //angular accelearation components in world coordinate system
                           double omegadoty,    //angular accelearation components in world coordinate system
                           double omegadotz,    //angular accelearation components in world coordinate system
                           double omegax,       //angular velocity components in world coordinate system
                           double omegay,       //angular velocity components in world coordinate system
                           double omegaz,       //angular velocity components in world coordinate system
                           double quaternion_x, //orientation quaternion components in world coordinate system
                           double quaternion_y, //orientation quaternion components in world coordinate system
                           double quaternion_z, //orientation quaternion components in world coordinate system
                           double quaternion_w  //orientation quaternion components in world coordinate system
    );
    void set_current_state_body_axis(
        double ax, double ay, double az,                      // linear acceleration component in body coordinate system
        double vx, double vy, double vz,                      // linear velocity component in body coordinate system
        double wind_vx, double wind_vy, double wind_vz,       // wind linear velocity component in body coordinate system
        double omegadotx, double omegadoty, double omegadotz, // angular accelearation components in body coordinate system
        double omegax, double omegay, double omegaz,          // angular velocity components in body coordinate system
        double yaw, double pitch, double roll,                // radians
        double alpha_rads,                                    // AoA radians
        double beta_rads,                                     //AoS radians
        double dT,
        double ro_kgm3);
    // Removed: set_qbar, set_bi2vel, set_ci2vel, UpdateWindMatrices
    // These are now computed by Auxiliary::Run() via LoadInputs in the normal model loop.

    bool caught_wire_ft(double t, const double pt[4][3])
    {
        return false;
    }

    bool get_wire_ends_m(double t, double end[2][3], double vel[2][3])
    {
        return false;
    }

    bool get_wire_ends_ft(double t, double end[2][3], double vel[2][3])
    {
        return false;
    }

    void release_wire(void)
    {
    }

    bool needTrim;
    double Wingspan;
    double Wingchord;

    //private:
    JSBSim::FGFDMExec* fdmex;
    std::shared_ptr<JSBSim::FGInitialCondition> fgic;
    std::shared_ptr<JSBSim::FGAtmosphere> Atmosphere;
    std::shared_ptr<JSBSim::FGWinds> Winds;
    std::shared_ptr<JSBSim::FGFCS> FCS;
    std::shared_ptr<JSBSim::FGPropulsion> Propulsion;
    std::shared_ptr<JSBSim::FGMassBalance> MassBalance;
    std::shared_ptr<JSBSim::FGAircraft> Aircraft;
    std::shared_ptr<JSBSim::FGPropagate> Propagate;
    std::shared_ptr<JSBSim::FGAuxiliary> Auxiliary;
    std::shared_ptr<JSBSim::FGAerodynamics> Aerodynamics;
    std::shared_ptr<JSBSim::FGGroundReactions> GroundReactions;
    //JSBSim::FGInertial*        Inertial;
    //JSBSim::FGAccelerations*   Accelerations;
    JSBSim::FGPropertyManager* PropertyManager;


    bool crashed;

    // --- debug instrumentation property nodes ---
    struct DebugNodes {
        SGPropertyNode* flag;
        // timing (set by DCS_interface)
        SGPropertyNode* dt_dcs;
        SGPropertyNode* wallclock_us;
        SGPropertyNode* wall_dt_us;
        SGPropertyNode* frame_count;
        // forces (set by DCS_interface)
        SGPropertyNode* fbx;
        SGPropertyNode* fby;
        SGPropertyNode* fbz;
        // moments (set by DCS_interface)
        SGPropertyNode* ml;
        SGPropertyNode* mm;
        SGPropertyNode* mn;
        // engines (set by DCS_interface)
        SGPropertyNode* num_engines;
        SGPropertyNode* throttle[MAX_ENGINES];
        SGPropertyNode* thrust[MAX_ENGINES];
        SGPropertyNode* n1[MAX_ENGINES];
        // atmosphere (set by DCS_interface)
        SGPropertyNode* density_slugft3;
        SGPropertyNode* pressure_lbfft2;
        SGPropertyNode* altitude_ft;
        SGPropertyNode* temperature_R;
        SGPropertyNode* speed_of_sound_fps;
        // aero state (set by JSBSim_interface)
        SGPropertyNode* alpha_deg;
        SGPropertyNode* beta_deg;
        SGPropertyNode* alphadot;
        SGPropertyNode* betadot;
        SGPropertyNode* qbar;
        SGPropertyNode* vt_fps;
        SGPropertyNode* vt_ms;
        SGPropertyNode* vc_kts;
        SGPropertyNode* bi2vel;
        SGPropertyNode* ci2vel;
        SGPropertyNode* p;
        SGPropertyNode* q;
        SGPropertyNode* r;
        SGPropertyNode* density_kgm3;
        SGPropertyNode* pitch;
        SGPropertyNode* roll;
        SGPropertyNode* yaw;
        // source nodes for reading JSBSim values
        SGPropertyNode* src_fbx;
        SGPropertyNode* src_fby;
        SGPropertyNode* src_fbz;
        SGPropertyNode* src_ml;
        SGPropertyNode* src_mm;
        SGPropertyNode* src_mn;
        SGPropertyNode* src_throttle[MAX_ENGINES];
        SGPropertyNode* src_thrust[MAX_ENGINES];
        SGPropertyNode* src_n1[MAX_ENGINES];
        SGPropertyNode* src_adot;
        SGPropertyNode* src_bdot;
    } dbg;
    void initDebugNodes();

    void init_gear(void);
    void update_gear(void);

    void update_external_forces(double t_off);
};


#endif // _JSBSIM_HXX
