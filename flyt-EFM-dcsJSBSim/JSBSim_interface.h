#ifndef _JSBSIM_HXX
#define _JSBSIM_HXX

/*%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
INCLUDES
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%*/

#undef MAX_ENGINES
#define MAX_ENGINES 4
#include "math/FGColumnVector3.h"
/*%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
DEFINITIONS
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%*/

#define ID_JSBSIMXX "$Header JSBSim.hxx,v 1.4 2000/10/22 14:02:16 jsb Exp $"

#define METERS_TO_FEET 3.2808398950
#define RADTODEG 57.2957795

/*%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
FORWARD DECLARATIONS
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%*/

#include <simgear/props/props.hxx>

#include "FGFDMExec.h"
#define METER_TO_FEET_FACTOR 3.28084
#define FEET_TO_METER_FACTOR   0.3048
#define DEGREES_TO_RADIANS 0.0174532925
#define RADIANS_TO_DEGREES 57.295779505
#define JSB_NEARLY_ZERO 0.0000001 // avoid division by zero
#define LBF_TO_NM 1.3558179483314
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
class FGSurfaces
{
public:
    double aileron[2];
    double get_aileron(int i) { return aileron[i]; }
    void set_aileron(int i, double v) { aileron[i] = v; }

    double elevator;
    double get_elevator() { return elevator; }
    void set_elevator(double v) { elevator = v; }

    double rudder;
    double get_rudder() { return rudder; }
    void set_rudder(double v) { rudder = v; }

    double flaps[2];
    double get_flaps(int i) { return flaps[i]; }
    void set_flaps(int i, double v) { flaps[i] = v; }

    double slats[2];
    double get_slats(int i) { return slats[i]; }
    void set_slats(int i, double v) { slats[i] = v; }

    double speedbrake[2];
    double get_speedbrake(int i) { return speedbrake[i]; }
    void set_speedbrake(int i, double v) { speedbrake[i] = v; }

    double gear[3];
    double get_gear(int i) { return gear[i]; }
    void set_gear(int i, double v) { gear[i] = v; }
    
    #define NUM_ENGINES  2

    double n1_percent[NUM_ENGINES];
    double get_n1_percent(int i) { return n1_percent[i]; }
    void set_n1_percent(int i, double v) { n1_percent[i] = v; }

    double thrust_N[NUM_ENGINES];
    double get_thrust_N(int i) { return thrust_N[i]; }
    void set_thrust_N(int i, double v) { thrust_N[i] = v; }

};
class FGControls
{
public:
    double get_aileron() { return _aileron; }
    double get_aileron_trim() { return _aileron_trim; }
    double get_augmentation(int e) { return _augmentation[e]; }
    double get_brake_left() { return _brake_left; }
    double get_brake_parking() { return _brake_parking; }
    double get_brake_right() { return _brake_right; }
    double get_condition(int e) { return _condition[e]; }
    double get_cutoff(int e) { return _cutoff[e]; }
    double get_elevator() { return _elevator; }
    double get_elevator_trim() { return _elevator_trim; }
    double get_feather(int e) { return _feather[e]; }
    double get_flaps() { return _flaps; }
    double get_gear_down() { return _gear_down; }
    double get_generator_breaker(int e) { return _generator_breaker[e]; }
    double get_ignition(int e) { return _ignition[e]; }
    double get_magnetos(int e) { return _magnetos[e]; }
    double get_mixture(int e) { return _mixture[e]; }
    double get_prop_advance(int e) { return _prop_advance[e]; }
    double get_reverser(int e) { return _reverser[e]; }
    double get_rudder() { return _rudder; }
    double get_rudder_trim() { return _rudder_trim; }
    double get_speedbrake() { return _speedbrake; }
    double get_spoilers() { return _spoilers; }
    double get_starter(int e) { return _starter[e]; }
    double get_throttle(int e) { return _throttle[e]; }
    void set_aileron(double val) { _aileron = val; }
    void set_elevator(double val) { _elevator = val; }
    void set_elevator_trim(double val) { _elevator_trim = val; }
    void set_magnetos(int e, double val) { _magnetos[e] = val; }
    void set_mixture(int e, double val) { _mixture[e] = val; }
    void set_rudder(double val) { _rudder = val; }
    void set_throttle(int e, double val) { _throttle[e] = val; }
    double _aileron;
    double _aileron_trim;
    double _augmentation[MAX_ENGINES];
    double _brake_left;
    double _brake_parking;
    double _brake_right;
    double _condition[MAX_ENGINES];
    double _cutoff[MAX_ENGINES];
    double _elevator;
    double _elevator_trim;
    double _feather[MAX_ENGINES];
    double _flaps;
    double _gear_down;
    double _generator_breaker[MAX_ENGINES];
    double _ignition[MAX_ENGINES];
    double _magnetos[MAX_ENGINES];
    double _mixture[MAX_ENGINES];
    double _prop_advance[MAX_ENGINES];
    double _reverser[MAX_ENGINES];
    double _rudder;
    double _rudder_trim;
    double _speedbrake;
    double _spoilers;
    double _starter[MAX_ENGINES];
    double _throttle[MAX_ENGINES];

};


class FGJSBsim {

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

    double fgGetDouble(const char * name, double defaultValue=0.0)
    {
        return PropertyManager->GetNode()->getDoubleValue(name, defaultValue);
    }
    double fgSetDouble(const char * name, double value)
    {
        return PropertyManager->GetNode()->setDoubleValue(name, value);
    }
    double fgGetBool(const char * name, bool defaultValue=false)
    {
        return PropertyManager->GetNode()->getBoolValue(name, defaultValue);
    }
    double fgSetBool(const char * name, bool value)
    {
        return PropertyManager->GetNode()->setBoolValue(name, value);
    }
    bool caught_wire_m(double t, const double pt[4][3])
    {
        return false;
    }
    void set_roll_pitch_yaw(double roll, double pitch, double yaw);
    void set_velocities_w_aero_fps(double v);
    void set_velocities_v_aero_fps(double v);
    void set_velocities_vt_fps(double v);
    void set_velocities_u_aero_fps(double v);
    void set_velocities_r_aero_rad_sec(double v);
    void set_velocities_q_aero_rad_sec(double v);
    void set_velocities_p_aero_rad_sec(double v);
    void set_velocities_mach(double v);
    void set_fcs_elevator_cmd_norm(double v);
    void set_fcs_aileron_cmd_norm(double v);
    void set_fcs_rudder_cmd_norm(double v);
    void set_fcs_throttle_cmd_norm(int e, double v);
    void set_fcs_gear_cmd_norm(double v);
    double get_pressure_at_atltiude_lbf_ft2(double altitude) const;
    void set_atmosphere_rho_slugs_ft3(double v);
    double get_atmosphere_rho_slugs_ft3();
    void set_atmosphere_pressure_lbf_ft2(double v);
    double get_atmosphere_pressure_lbf_ft2();
    const JSBSim::FGColumnVector3& GetXYZcg(void);
    const JSBSim::FGColumnVector3& GetXYZrp(void);
    void set_sound_speed(double v);
    void set_altitude(double h_ft);
    void set_aero_beta_deg(double v);
    void set_aero_betadot_rad_sec(double v);
    void set_aero_alpha_deg(double v);
    void set_aero_alphadot_rad_sec(double v);
    void set_qbar(double v);
    void set_bi2vel(double v);
    void set_ci2vel(double v);
    void UpdateWindMatrices();

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
    FGControls controls;

    FGControls *get_controls()
    {
        return &controls;
    }
    FGSurfaces surfaces;
    FGSurfaces *get_surfaces() { return &surfaces; }
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

    // disabling unused members
    /*
    int runcount;
    double trim_elev;
    double trim_throttle;
    */

    //SGPropertyNode_ptr startup_trim;
    //SGPropertyNode_ptr trimmed;
    //SGPropertyNode_ptr pitch_trim;
    //SGPropertyNode_ptr throttle_trim;
    //SGPropertyNode_ptr aileron_trim;
    //SGPropertyNode_ptr rudder_trim;
    //SGPropertyNode_ptr stall_warning;

    /* SGPropertyNode_ptr elevator_pos_deg;
    SGPropertyNode_ptr left_aileron_pos_deg;
    SGPropertyNode_ptr right_aileron_pos_deg;
    SGPropertyNode_ptr rudder_pos_deg;
    SGPropertyNode_ptr flap_pos_deg; */


    //SGPropertyNode_ptr elevator_pos_pct;
    //SGPropertyNode_ptr left_aileron_pos_pct;
    //SGPropertyNode_ptr right_aileron_pos_pct;
    //SGPropertyNode_ptr rudder_pos_pct;
    //SGPropertyNode_ptr flap_pos_pct;
    //SGPropertyNode_ptr speedbrake_pos_pct;
    //SGPropertyNode_ptr spoilers_pos_pct;

    //SGPropertyNode_ptr ab_brake_engaged;
    //SGPropertyNode_ptr ab_brake_left_pct;
    //SGPropertyNode_ptr ab_brake_right_pct;

    //SGPropertyNode_ptr gear_pos_pct;
    //SGPropertyNode_ptr wing_fold_pos_pct;
    //SGPropertyNode_ptr tailhook_pos_pct;

    //SGPropertyNode_ptr altitude;
    //SGPropertyNode_ptr agl_ft;
    //SGPropertyNode_ptr temperature;
    //SGPropertyNode_ptr pressure;
    //SGPropertyNode_ptr pressureSL;
    //SGPropertyNode_ptr ground_wind;
    //SGPropertyNode_ptr turbulence_gain;
    //SGPropertyNode_ptr turbulence_rate;
    //SGPropertyNode_ptr turbulence_model;

    //SGPropertyNode_ptr wind_from_north;
    //SGPropertyNode_ptr wind_from_east;
    //SGPropertyNode_ptr wind_from_down;

    //SGPropertyNode_ptr arrestor_wire_engaged_hook;
    //SGPropertyNode_ptr release_hook;

    //SGPropertyNode_ptr slaved;

    //SGPropertyNode_ptr terrain;

    //static std::map<std::string, int> TURBULENCE_TYPE_NAMES;

    //double last_hook_tip[3];
    //double last_hook_root[3];
    //JSBSim::FGColumnVector3 hook_root_struct;
    //double hook_length;

    bool crashed;

    //SGPropertyNode_ptr _ai_wake_enabled;
    //SGPropertyNode_ptr _fmag{ nullptr }, _mmag{ nullptr };
    //SGPropertyNode_ptr _fbx{ nullptr }, _fby{ nullptr }, _fbz{ nullptr };
    //SGPropertyNode_ptr _mbx{ nullptr }, _mby{ nullptr }, _mbz{ nullptr };

    //void do_trim(void);

    //bool update_ground_cache(const JSBSim::FGLocation& cart, double dt);
    void init_gear(void);
    void update_gear(void);

    void update_external_forces(double t_off);
};


#endif // _JSBSIM_HXX
