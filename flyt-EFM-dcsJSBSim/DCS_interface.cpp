#include "stdafx.h"
#include <Windows.h>

#include "DCS_interface.h"
#include "fm_math.h"
#include "math/FGQuaternion.h"
#include "models/FGPropulsion.h"
#include <string>
Vec3d center_of_mass;                    // m
Vec3d common_moment;                     // kg/m^2
Vec3d common_force;                      // N

extern FGJSBsim* get_model();

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


void DCS_interface::initialize(FGJSBsim * _model)
{
    printf("acEFM: DCS_interface Initialize\n");

    auto drawArgNode = _model->PropertyManager->GetNode("sim/animations");
    if (drawArgNode != nullptr) {
        for (auto drawarg : drawArgNode->getChildren("drawarg")) {
            auto prop = drawarg->getStringValue("property");
            auto factor = drawarg->getFloatValue("factor", 1.0);
            auto delta = drawarg->getFloatValue("delta", 0.0001);
            drawArguments.AddItem(drawarg->getIndex(), prop, factor, delta);
            ;
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
            auto delta = gauge->getDoubleValue("delta", 0.0001);
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
    FGJSBsim* model = get_model();
    if (index < ED_FM_END_ENGINE_BLOCK) {
        // engine block: each DCS engine occupies a block of 100 indices
        // DCS engine 0 = APU, DCS engine N (N>=1) = JSBSim engine[N-1]
        static const unsigned ENGINE_BLOCK_SIZE = ED_FM_ENGINE_1_RPM - ED_FM_ENGINE_0_RPM;
        unsigned dcs_engine  = index / ENGINE_BLOCK_SIZE;
        unsigned param_offset = index % ENGINE_BLOCK_SIZE;

        if (dcs_engine == 0)
            return 0; // APU - not modelled

        int jsb_engine = (int)dcs_engine - 1;
        if (jsb_engine >= (int)model->Propulsion->GetNumEngines())
            return 0;

        // build property path for this JSBSim engine
        std::string eng_prefix = "/fdm/jsbsim/propulsion/engine[" + std::to_string(jsb_engine) + "]/";

        switch (param_offset) {
        case ED_FM_ENGINE_0_RPM:            // offset 0: RPM
            return model->fgGetDouble((eng_prefix + "n1").c_str()) * 120;
        case ED_FM_ENGINE_0_RELATED_RPM:    // offset 1: related RPM (N2 as 0-1)
            return model->fgGetDouble((eng_prefix + "n2").c_str());
        case ED_FM_ENGINE_0_THRUST:         // offset 4: thrust in N
            return model->fgGetDouble((eng_prefix + "thrust-lbs").c_str()) * LBS_TO_N;
        case ED_FM_ENGINE_0_RELATED_THRUST: // offset 5
            return 0;
        case ED_FM_ENGINE_0_FUEL_FLOW:      // offset 14
            return model->fgGetDouble((eng_prefix + "fuel-flow-rate-pps").c_str());
        default:
            return 0;
        }
    } else if (index >= ED_FM_SUSPENSION_0_RELATIVE_BRAKE_MOMENT &&
               index < ED_FM_OXYGEN_SUPPLY) {
        // gear/suspension block
        switch (index) {
        case ED_FM_SUSPENSION_0_GEAR_POST_STATE:
        case ED_FM_SUSPENSION_0_DOWN_LOCK:
        case ED_FM_SUSPENSION_0_UP_LOCK:
            return model->fgGetDouble("/fdm/jsbsim/gear/unit[0]/pos-norm");
        case ED_FM_SUSPENSION_1_GEAR_POST_STATE:
        case ED_FM_SUSPENSION_1_DOWN_LOCK:
        case ED_FM_SUSPENSION_1_UP_LOCK:
            return model->fgGetDouble("/fdm/jsbsim/gear/unit[1]/pos-norm");
        case ED_FM_SUSPENSION_2_GEAR_POST_STATE:
        case ED_FM_SUSPENSION_2_DOWN_LOCK:
        case ED_FM_SUSPENSION_2_UP_LOCK:
            return model->fgGetDouble("/fdm/jsbsim/gear/unit[2]/pos-norm");
        }

        static const int block_size =
            ED_FM_SUSPENSION_1_RELATIVE_BRAKE_MOMENT -
            ED_FM_SUSPENSION_0_RELATIVE_BRAKE_MOMENT;
        switch (index) {
        case 0 * block_size + ED_FM_SUSPENSION_0_GEAR_POST_STATE:
            //return A37::Systems::Gear::noseGearValue;
        case 1 * block_size + ED_FM_SUSPENSION_0_GEAR_POST_STATE:
            //return A37::Systems::Gear::leftGearValue;
        case 2 * block_size + ED_FM_SUSPENSION_0_GEAR_POST_STATE:
            //return A37::Systems::Gear::rightGearValue;
            return 0;
        }
    } else if (index >= ED_FM_SUSPENSION_0_RELATIVE_BRAKE_MOMENT &&
               index < ED_FM_OXYGEN_SUPPLY) {
        static const int block_size =
            ED_FM_SUSPENSION_1_RELATIVE_BRAKE_MOMENT -
            ED_FM_SUSPENSION_0_RELATIVE_BRAKE_MOMENT;
        switch (index) {
        case 0 * block_size + ED_FM_SUSPENSION_0_GEAR_POST_STATE:
            //return A37::Systems::Gear::noseGearValue;
        case 1 * block_size + ED_FM_SUSPENSION_0_GEAR_POST_STATE:
            //return A37::Systems::Gear::leftGearValue;
        case 2 * block_size + ED_FM_SUSPENSION_0_GEAR_POST_STATE:
            //return A37::Systems::Gear::rightGearValue;
            return 0;
        }
    } else
        switch (index) {
            //ED_FM_CAN_ACCEPT_FUEL_FROM_TANKER,// return positive value if all conditions are matched to connect to tanker and get fuel
            //ED_FM_ENGINE_1_RPM = 100,
            //ED_FM_ENGINE_1_RELATED_RPM,
            //ED_FM_ENGINE_1_CORE_RPM,
            //ED_FM_ENGINE_1_CORE_RELATED_RPM,
            //ED_FM_ENGINE_1_THRUST,
            //ED_FM_ENGINE_1_RELATED_THRUST,
            //ED_FM_ENGINE_1_CORE_THRUST,
            //ED_FM_ENGINE_1_CORE_RELATED_THRUST,
            //
            //ED_FM_PROPELLER_1_RPM,    // propeller RPM , for helicopter it is main rotor RPM
            //ED_FM_PROPELLER_1_PITCH,  // propeller blade pitch
            //ED_FM_PROPELLER_1_TILT,   // for helicopter
            //ED_FM_PROPELLER_1_INTEGRITY_FACTOR,   // for 0 to 1 , 0 is fully broken ,
            //
            //ED_FM_ENGINE_1_TEMPERATURE,//Celcius
            //ED_FM_ENGINE_1_OIL_PRESSURE,
            //ED_FM_ENGINE_1_FUEL_FLOW,
            //ED_FM_ENGINE_1_COMBUSTION,//level of combustion for engine  , 0 - 1
            //
            //ED_FM_PISTON_ENGINE_1_MANIFOLD_PRESSURE,
            //ED_FM_SUSPENSION_0_RELATIVE_BRAKE_MOMENT,
            //ED_FM_SUSPENSION_0_GEAR_POST_STATE, // from 0 to 1 : from fully retracted to full released
            //ED_FM_SUSPENSION_0_UP_LOCK,
            //ED_FM_SUSPENSION_0_DOWN_LOCK,
            //ED_FM_SUSPENSION_0_WHEEL_YAW,
            //ED_FM_SUSPENSION_0_WHEEL_SELF_ATTITUDE,
            //
            //ED_FM_SUSPENSION_1_RELATIVE_BRAKE_MOMENT = ED_FM_SUSPENSION_0_RELATIVE_BRAKE_MOMENT + 10,
            //ED_FM_SUSPENSION_1_GEAR_POST_STATE, // from 0 to 1 : from fully retracted to full released
            //ED_FM_SUSPENSION_1_UP_LOCK,
            //ED_FM_SUSPENSION_1_DOWN_LOCK,
            //ED_FM_SUSPENSION_1_WHEEL_YAW,
            //ED_FM_SUSPENSION_1_WHEEL_SELF_ATTITUDE,
            //
            //
            //ED_FM_SUSPENSION_2_RELATIVE_BRAKE_MOMENT = ED_FM_SUSPENSION_1_RELATIVE_BRAKE_MOMENT + (ED_FM_SUSPENSION_1_RELATIVE_BRAKE_MOMENT - ED_FM_SUSPENSION_0_RELATIVE_BRAKE_MOMENT),
            //ED_FM_SUSPENSION_2_GEAR_POST_STATE, // from 0 to 1 : from fully retracted to full released
            //ED_FM_SUSPENSION_2_UP_LOCK,
            //ED_FM_SUSPENSION_2_DOWN_LOCK,
            //ED_FM_SUSPENSION_2_WHEEL_YAW,
            //ED_FM_SUSPENSION_2_WHEEL_SELF_ATTITUDE,

            //    ED_FM_FUEL_FUEL_TANK_GROUP_0_LEFT,			// Values from different group of tanks
            //    ED_FM_FUEL_FUEL_TANK_GROUP_0_RIGHT,
            //    ED_FM_FUEL_FUEL_TANK_GROUP_1_LEFT,
            //    ED_FM_FUEL_FUEL_TANK_GROUP_1_RIGHT,
            //    ED_FM_FUEL_FUEL_TANK_GROUP_2_LEFT,
            //    ED_FM_FUEL_FUEL_TANK_GROUP_2_RIGHT,
            //    ED_FM_FUEL_FUEL_TANK_GROUP_3_LEFT,
            //    ED_FM_FUEL_FUEL_TANK_GROUP_3_RIGHT,
            //    ED_FM_FUEL_FUEL_TANK_GROUP_4_LEFT,
            //    ED_FM_FUEL_FUEL_TANK_GROUP_4_RIGHT,
            //    ED_FM_FUEL_FUEL_TANK_GROUP_5_LEFT,
            //    ED_FM_FUEL_FUEL_TANK_GROUP_5_RIGHT,
            //    ED_FM_FUEL_FUEL_TANK_GROUP_6_LEFT,
            //    ED_FM_FUEL_FUEL_TANK_GROUP_6_RIGHT,
            //    ED_FM_FUEL_INTERNAL_FUEL,
            //    ED_FM_FUEL_TOTAL_FUEL,
            //    ED_FM_FUEL_LOW_SIGNAL,						// Low fuel signal
            //        ED_FM_ANTI_SKID_ENABLE,// return positive value if anti skid system is on , it also corresspond with suspension table "anti_skid_installed"  parameter for each gear post .i.e
        case ED_FM_FLOW_VELOCITY:
            break;
        case ED_FM_OXYGEN_SUPPLY: // oxygen provided from on board oxygen system, pressure - pascal)
            break;

        // additional pressure from pressurization system , pascal , actual cabin pressure will be AtmoPressure + COCKPIT_PRESSURIZATION_OVER_EXTERNAL
        case ED_FM_COCKPIT_PRESSURIZATION_OVER_EXTERNAL: {
            double cabin_alt = model->fgGetDouble("/fdm/jsbsim/systems/ecs/cabin-altitude-ft");
            double cabin_pressure = model->get_pressure_at_atltiude_lbf_ft2(cabin_alt);
            double diff = (cabin_pressure - model->get_atmosphere_pressure_lbf_ft2()) / PA_TO_LBF_FT2;
            //        printf("cabin alt: %.1f pres %.1f ext %.1f diff %.1f \n", cabin_alt, cabin_pressure, model->get_atmosphere_pressure_lbf_ft2(), diff);
            return diff;
        } break;
        }
    return 0;
}


void ed_fm_add_local_force(double &x, double &y, double &z, double &pos_x, double &pos_y, double &pos_z)
{
    FGJSBsim *model = get_model();
    x = model->fgGetDouble("/fdm/jsbsim/forces/fbx-total-lbs") * LBS_TO_N;
    y = -model->fgGetDouble("/fdm/jsbsim/forces/fbz-total-lbs") * LBS_TO_N;
    z = model->fgGetDouble("/fdm/jsbsim/forces/fby-total-lbs") * LBS_TO_N;

    pos_x = center_of_mass.x;
    pos_y = center_of_mass.y;
    pos_z = center_of_mass.z;
}

void ed_fm_add_local_moment(double &x, double &y, double &z)
{
    FGJSBsim *model = get_model();

    // jsb
    // x + back
    // y + right
    // z + up

    // dcs
    // x + forward
    // y + up
    // z + right

    x = model->fgGetDouble("/fdm/jsbsim/moments/l-total-lbsft") * LBSFT_TO_NM;
    y = -model->fgGetDouble("/fdm/jsbsim/moments/n-total-lbsft") * LBSFT_TO_NM;
    z = model->fgGetDouble("/fdm/jsbsim/moments/m-total-lbsft") * LBSFT_TO_NM;
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
#ifndef _DEBUG
    try
    {
#endif
        FGJSBsim* model = get_model();
        // Convert units
        double density_slugs_ft3 = ro_kgm3 * KGM3_TO_SLUGS_FT3;
        double pressure_psf = p_Pa * PA_TO_LBF_FT2;
        double soundspeed_fps = a * METERS_TO_FEET;
        double altitude_ft = h * METERS_TO_FEET;
        double temp_rankine = t * 1.8; // Kelvin to Rankine

        // Sea level 15degres:
        // 1.225 kg / m3
        // 1.225 x10-3 g/cm3,
        // 0.0023769 slug / (cu ft), 
        // 0.0765 lb / (cu ft)
        //printf("ed_fm_set_atmos %f, %f, %f, %f\n", h, t, a, ro);
        model->set_atmosphere_rho_slugs_ft3(ro_kgm3 * KGM3_TO_SLUGS_FT3);
        model->set_atmosphere_pressure_lbf_ft2(p_Pa * PA_TO_LBF_FT2);

        model->set_sound_speed(a * METERS_TO_FEET);
        model->set_altitude(h * METERS_TO_FEET);

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
   mass = mass;
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
    FGJSBsim *model = get_model();
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
    static int init = 0;
    static double last_alpha_rads;
    static double last_beta_rads;

    FGJSBsim *model = get_model();
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
bool ed_fm_change_mass(double &delta_mass,
    double &delta_mass_pos_x,
    double &delta_mass_pos_y,
    double &delta_mass_pos_z,
    double &delta_mass_moment_of_inertia_x,
    double &delta_mass_moment_of_inertia_y,
    double &delta_mass_moment_of_inertia_z)
{
    return false;
    // original demo code
    /*
    if (fuel_consumption_since_last_time > 0)
    {
    delta_mass                = fuel_consumption_since_last_time;
    delta_mass_pos_x = -1.0;
    delta_mass_pos_y =  1.0;
    delta_mass_pos_z =  0;

    delta_mass_moment_of_inertia_x   = 0;
    delta_mass_moment_of_inertia_y   = 0;
    delta_mass_moment_of_inertia_z   = 0;

    fuel_consumption_since_last_time = 0; // set it 0 to avoid infinite loop, because it called in cycle
    // better to use stack like structure for mass changing
    return true;
    }
    else
    {
    return false;
    }
    */
}

/*
set internal fuel volume , init function, called on object creation and for refueling ,
you should distribute it inside at different fuel tanks
*/
void ed_fm_set_internal_fuel(double fuel)
{
    //printf("ed_fm_set_internal_fuel %5.1f\n", fuel);
}

double ed_fm_get_internal_fuel()
{
    //printf("ed_fm_get_internal_fuel\n");
    return 0;
}

/*
set external fuel volume for each payload station , called for weapon init and on reload
*/
void ed_fm_set_external_fuel(int station, double fuel, double x, double y, double z)
{
    //printf("ed_fm_set_internal_fuel %d %5.1f (%5.1f,%5.1f,%5.1f)\n", station, fuel, x,y,z);
}

double ed_fm_get_external_fuel()
{
    //printf("ed_fm_get_external_fuel\n");
    return 0;
}

void ed_fm_configure(const char * cfg_path)
{
    printf("ed_fm_configure %s\n", cfg_path);
}

void ed_fm_set_plugin_data_install_path(const char* cfg_path)
{
    printf("ed_fm_set_plugin_data_install_path: %s\n", cfg_path);
    root_folder = std::string(cfg_path);
}

void ed_fm_suspension_feedback(int idx, const ed_fm_suspension_info * info)
{
    if (true)
    {
//        std::cout << "suspension info " << idx << ": " << info->struct_compression << "\t" << info->integrity_factor << "\t" <<
//        info->acting_force[0] << ", " << info->acting_force[1] << ", " << info->acting_force[2] << "\n";
    }
}

void ed_fm_set_surface(double h,	//surface height under the center of aircraft
    double h_obj,	//surface height with objects
    unsigned surface_type, double normal_x,	//components of normal vector to surface
    double normal_y,	//components of normal vector to surface
    double normal_z	//components of normal vector to surface
)
{
    //printf("ed_fm_set_surface %5.1f %5.1f %d %5.1f,%5.1f,%5.1f\n", h, h_obj, surface_type, normal_x, normal_y, normal_z);
    //model->set_agl_m(h);
    
}

/*
input handling
*/
void ed_fm_set_command(int command, float value)
{
    FGJSBsim *model = get_model();
    if (command == 2004) { //iCommandPlaneThrustCommon
        // +1 off
        // -1 full
        value = (-value + 1.0) / 2.0;
        for (unsigned int e = 0; e < model->Propulsion->GetNumEngines(); e++)
            model->set_fcs_throttle_cmd_norm(e, value);
    }
    else if (command == 2005) { //iCommandPlaneThrustLeft
        value = (-value + 1.0) / 2.0;
        model->set_fcs_throttle_cmd_norm(0, value);
    }
    else if (command == 2006) { //iCommandPlaneThrustRight
        value = (-value + 1.0) / 2.0;
        model->set_fcs_throttle_cmd_norm(1, value);
    }
    else if (command == 2001) { //iCommandPlanePitch
        double ep = -value;// 0.5 * (-value + 1.0);
        model->set_fcs_elevator_cmd_norm(ep);
        model->dbg.elevator_cmd->setDoubleValue(ep);
    }
    else if (command == 2002) { //iCommandPlaneRoll{
        model->set_fcs_aileron_cmd_norm(value);
        model->dbg.aileron_cmd->setDoubleValue(value);
    }
    else if (command == 2003) { //iCommandPlaneYaw{
        model->set_fcs_rudder_cmd_norm(value);
        model->dbg.rudder_cmd->setDoubleValue(value);
    }
    //else if (command == 2035) {
    //    printf("Gear %d -> %.2f\n", command, value);
    //    model->set_fcs_gear_cmd_norm(value);
    //}
    else if (command == 68 || command == 1609) {
        model->set_fcs_gear_cmd_norm(value);
    }
    else if (command == 431) {
        model->set_fcs_gear_cmd_norm(1.0);
    }
    else if (command == 430) {
        model->set_fcs_gear_cmd_norm(0);
    }
    if (command < 2000)
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

