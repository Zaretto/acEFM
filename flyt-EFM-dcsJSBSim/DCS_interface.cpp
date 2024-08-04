#include "stdafx.h"
#include <Windows.h>

#include "DCS_interface.h"
#include "fm_math.h"
#include "math/FGQuaternion.h"
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

double Vt = 0;
double Vt_ms = 0;
double qbar_m = 0; // Dynamic pressure (Pa == N/m^2) (velocity pressure)
double twovel_m;
double bi2vel_m;
double ci2vel_m;
int local_debug = 1;
int use_metric_forces = 1;
int use_metric_moments = 1;

double wind_x;
double wind_y;
double wind_z;
double dT;

double wing_area_m = 17.401;
double wing_span_m = 6.68;
double wing_chord_m = 2.605;

int frame_count = 0;
int nullf() { return 0; }

static std::map<std::string, double> conversion_map = {
{"METER_TO_FEET", 3.28084},
{"FEET_TO_METER  ", 0.3048},
{"DEGREES_TO_RADIANS", 0.0174532925},
{"RADIANS_TO_DEGREES", 57.295779505},
{"ZERO", 0.0000001}, // avoid division by zero
{"LBF_TO_NM", 1.3558179483314},
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
        use_metric_forces = model->fgGetDouble("/fdm/jsbsim/acefm/metric_forces");
        use_metric_moments = model->fgGetDouble("/fdm/jsbsim/acefm/metric_moments");
        frame_count = 0;
    }

    if (cockpit_param.pfn_ed_cockpit_update_parameter_with_number)
    {
        cockpit.Update(model);
    }
}

double ed_fm_get_param(unsigned index)
{
    FGJSBsim* model = get_model();
    if (index <= ED_FM_END_ENGINE_BLOCK) {
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

        case ED_FM_ENGINE_0_RPM:
        case ED_FM_ENGINE_0_RELATED_RPM:
        case ED_FM_ENGINE_0_THRUST:
        case ED_FM_ENGINE_0_RELATED_THRUST:
            return 0; // APU
        case ED_FM_ENGINE_1_RPM:
            return model->fgGetDouble("/fdm/jsbsim/propulsion/engine[0]/n1") * 120;
        case ED_FM_ENGINE_1_RELATED_RPM:
            return model->fgGetDouble("/fdm/jsbsim/propulsion/engine[0]/n2");
        case ED_FM_ENGINE_1_THRUST:
            return model->fgGetDouble("/fdm/jsbsim/propulsion/engine[0]/thrust-lbs") * LBS_TO_N;
        case ED_FM_ENGINE_1_RELATED_THRUST:
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

    double xx = -model->fgGetDouble("/fdm/jsbsim/aero/coefficients/CDRAG") * qbar_m * wing_area_m;
    double yy = model->fgGetDouble("/fdm/jsbsim/aero/coefficients/CLIFT") * qbar_m * wing_area_m;
    double zz = model->fgGetDouble("/fdm/jsbsim/aero/coefficients/CSIDE") * qbar_m * wing_area_m;
    if (local_debug)
        printf("Forces (%8.1f, %8.1f, %8.1f) => (%8.1f, %8.1f, %8.1f)", x, y, z, xx, yy, zz);
    if (use_metric_forces)
    {
        x = xx;
        y = yy;
        z = zz;
    }
        pos_x = center_of_mass.x;
    pos_y = center_of_mass.y;
    pos_z = center_of_mass.z;
}

void ed_fm_add_local_moment(double &x, double &y, double &z)
{
//#define FT_LB2_TO_KGM2   4.88242764

    FGJSBsim *model = get_model();

    // jsb
    // x + back
    // y + right
    // z + up

    // dcs
    // x + forward
    // y + up
    // z + right

    x = model->fgGetDouble("/fdm/jsbsim/moments/l-total-lbsft") / LBS_TO_N   ;
    y = -model->fgGetDouble("/fdm/jsbsim/moments/n-total-lbsft")/ LBS_TO_N   ;
    z = model->fgGetDouble("/fdm/jsbsim/moments/m-total-lbsft") / LBS_TO_N   ;

    double roll  = model->fgGetDouble("/fdm/jsbsim/aero/coefficients/CROLL");
    double yaw   = -model->fgGetDouble("/fdm/jsbsim/aero/coefficients/CYAW");
    double pitch = model->fgGetDouble("/fdm/jsbsim/aero/coefficients/CPITCH");


    double xx = roll  * qbar_m * wing_area_m * wing_span_m  / 9.8;
    double yy = yaw   * qbar_m * wing_area_m * wing_span_m  / 9.8;
    double zz = pitch * qbar_m * wing_area_m * wing_chord_m / 9.8;

    if (local_debug)
        printf("Moments (%7.1f, %7.1f, %7.1f) => (%7.1f, %7.1f, %7.1f) C(%7.4f, %7.4f, %7.4f) qb %7.1f\n", x, y, z, xx, yy, zz, roll, yaw, pitch, qbar_m);

    if (use_metric_moments)
    {
        x = xx;
        y = yy;
        z = zz;
    }
}

void ed_fm_set_atmosphere(double h,	//altitude above sea level
    double t,	//Ambient temperature (kelvin)
    double a,	//speed of sound
    double ro,	// (kg/m^3)
    double p,	// atmosphere pressure (Pa == N/m^2)
    double wind_vx,	//components of velocity vector, including turbulence in world coordinate system
    double wind_vy,	//components of velocity vector, including turbulence in world coordinate system
    double wind_vz	//components of velocity vector, including turbulence in world coordinate system
)
{
    try
    {
        FGJSBsim *model = get_model();

        // Sea level 15degres:
        // 1.225 kg / m3
        // 1.225 x10-3 g/cm3,
        // 0.0023769 slug / (cu ft), 
        // 0.0765 lb / (cu ft)
        //printf("ed_fm_set_atmos %f, %f, %f, %f\n", h, t, a, ro);
        model->set_atmosphere_rho_slugs_ft3(ro * KGM3_TO_SLUGS_FT3);
        model->set_atmosphere_pressure_lbf_ft2(p * PA_TO_LBF_FT2);

        model->set_sound_speed(a * METERS_TO_FEET);
        model->set_altitude(h * METERS_TO_FEET);
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
        //double densityD2 = 0.5*(ro * KGM3_TO_SLUGS_FT3);
        double twovel = Vt * 2;
        double   qbar = 0.5*(ro * KGM3_TO_SLUGS_FT3) * Vt*Vt;
        double bi2vel = model->Wingspan / twovel;
        double ci2vel = model->Wingchord / twovel;
        model->set_qbar(qbar);
        model->set_bi2vel(bi2vel);
        model->set_ci2vel(ci2vel);

        wind_x = wind_vx;
        wind_y = wind_vy;
        wind_z = wind_vz;


        twovel_m = Vt_ms * 2;
          qbar_m = 0.5*ro* Vt_ms*Vt_ms;
        bi2vel_m = wing_span_m / twovel;
        ci2vel_m = wing_chord_m / twovel;
        model->set_bi2vel(bi2vel_m);
        model->set_ci2vel(ci2vel_m);
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


    //        double p = pressure->getDoubleValue();
    //        double psl = fdmex->GetAtmosphere()->GetPressureSL();
    //        double rhosl = fdmex->GetAtmosphere()->GetDensitySL();
    //            double mach = FGJSBBase::MachFromVcalibrated(vc, p, psl, rhosl);
    //        double temp = 1.8*(temperature->getDoubleValue() + 273.15);

    //model->temperature->setDoubleValue(t);
    //model->pressure->setDoubleValue(p);
    //model->set_atmosphere_override_density(ro);
    //model->altitude->setDoubleValue(h);
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
    double common_angle_of_attack,                        // AoA radians
    double common_angle_of_slide	//AoS radians
)
{
    static int init = 0;
    static double last_alpha;
    static double last_beta;
    double alpha = common_angle_of_attack;
    double beta = common_angle_of_slide;
    FGJSBsim *model = get_model();

    model->set_aero_alpha_deg(alpha);
    model->set_aero_beta_deg(beta);

    if (init)
    {
        double factor = 1.0 / dT / RADIANS_TO_DEGREES;
        model->set_aero_alphadot_rad_sec(((last_alpha - alpha)) * factor);
        model->set_aero_betadot_rad_sec(((last_beta - beta)) * factor);
    }
    last_alpha = alpha;
    last_beta = beta;
    init = 1;

    //model->set_aero_alphadot_rad_sec(omegadotz);
    //model->set_aero_betadot_rad_sec(omegadoty);

    model->set_velocities_p_aero_rad_sec(omegax);
    model->set_velocities_q_aero_rad_sec(omegaz);
    model->set_velocities_r_aero_rad_sec(-omegay);

    model->set_velocities_u_aero_fps(vx * METER_TO_FEET_FACTOR);
    model->set_velocities_v_aero_fps(vz * METER_TO_FEET_FACTOR);
    model->set_velocities_w_aero_fps(-vy * METER_TO_FEET_FACTOR);

    /*
        attitude/phi-deg
        attitude/phi-rad
        attitude/pitch-rad
        attitude/psi-deg
        attitude/psi-rad
        attitude/roll-rad
        attitude/theta-deg
        attitude/theta-rad
        */
    Vt = Magnitude((vx - wind_x)* METER_TO_FEET_FACTOR, (vy - wind_y)* METER_TO_FEET_FACTOR, (vz - wind_z)* METER_TO_FEET_FACTOR);

    // include the effects of wind at this point on the velocities to get the overall forward velocity through the air mass.

    Vt_ms = Magnitude(vx-wind_x, vy - wind_y, vz - wind_z);
    model->set_velocities_vt_fps(Vt);

    model->UpdateWindMatrices();
    model->set_roll_pitch_yaw(roll, pitch, yaw);

    //    model->fgSetDouble("/fdm/jsbsim/attitude/phi-deg", 0);
    model->fgSetDouble("/fdm/jsbsim/attitude/phi-rad", roll);
//    model->fgSetDouble("/fdm/jsbsim/attitude/pitch-rad", pitch);
    //    model->fgSetDouble("/fdm/jsbsim/attitude/psi-deg", 0);
    model->fgSetDouble("/fdm/jsbsim/attitude/psi-rad", yaw);
//    model->fgSetDouble("/fdm/jsbsim/attitude/roll-rad", roll);
    model->fgSetDouble("/fdm/jsbsim/attitude/theta-deg", pitch * RADIANS_TO_DEGREES);

    //printf("Body angles (%.3f, %.3f, %.3f)\n",
    //model->fgGetDouble("/fdm/jsbsim/attitude/phi-deg"),
    //model->fgGetDouble("/fdm/jsbsim/attitude/theta-deg"),
    //    model->fgGetDouble("/fdm/jsbsim/attitude/psi-deg"));

    //    model->fgSetDouble("/fdm/jsbsim/attitude/theta-rad", pitch);
    //Auxiliary->Getbeta();
    //Auxiliary->Getqbar();
    //Auxiliary->GetVt();
    //Auxiliary->GetTb2w();
    //Auxiliary->GetTw2b();
    //MassBalance->StructuralToBody(Aircraft->GetXYZrp());

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

}

double ed_fm_get_internal_fuel()
{
    return 0;
}

/*
set external fuel volume for each payload station , called for weapon init and on reload
*/
void ed_fm_set_external_fuel(int station, double fuel, double x, double y, double z)
{
}

double ed_fm_get_external_fuel()
{
    return 0;
}

void ed_fm_configure(const char * cfg_path)
{
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
        //std::cout << "suspension info " << idx << ": " << info->struct_compression << "\t" << info->integrity_factor << "\t" <<
        //info->acting_force[0] << ", " << info->acting_force[1] << ", " << info->acting_force[2] << "\n";
    }
}

void ed_fm_set_surface(double h,	//surface height under the center of aircraft
    double h_obj,	//surface height with objects
    unsigned surface_type, double normal_x,	//components of normal vector to surface
    double normal_y,	//components of normal vector to surface
    double normal_z	//components of normal vector to surface
)
{
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
        //value = (1.0 - value)/2;
        value = (-value + 1.0) / 2.0;
        printf("Throttle %.2f\n", value);
        model->set_fcs_throttle_cmd_norm(0, value);
        //        model->get_controls()->set_throttle(1, value);
    }
    else if (command == 2001) { //iCommandPlanePitch
        double ep = -value;// 0.5 * (-value + 1.0);
            //printf("Elevator %.2f\n", ep);
        model->set_fcs_elevator_cmd_norm(ep);
        //        model->get_controls()->set_elevator(0.5 * (-value + 1.0));
    }
    else if (command == 2002) { //iCommandPlaneRoll{
        //printf("Aileron %.2f\n", value);
        model->set_fcs_aileron_cmd_norm(value);
    }
    else if (command == 2003) { //iCommandPlaneRoll{
                                //printf("Rudder %.2f\n", value);
        model->set_fcs_rudder_cmd_norm(value);
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

