#pragma once

#define DLL_API __declspec(dllexport)


#include "DCS-API/include/FM/wHumanCustomPhysicsAPI.h"
#include "JSBSim_interface.h"
#include "input_output/FGPropertyManager.h"

typedef void * (*PFN_ED_COCKPIT_GET_PARAMETER_HANDLE)  (const char * name);
typedef void(*PFN_ED_COCKPIT_UPDATE_PARAMETER_WITH_STRING)  (void		  * handle, const char * string_value);
typedef void(*PFN_ED_COCKPIT_UPDATE_PARAMETER_WITH_NUMBER)  (void		  * handle, double   number_value);
typedef bool(*PFN_ED_COCKPIT_PARAMETER_VALUE_TO_NUMBER)  (const void * handle, double & res, bool interpolated);
typedef bool(*PFN_ED_COCKPIT_PARAMETER_VALUE_TO_STRING)  (const void * handle, char * buffer, unsigned buffer_size);
typedef int(*PFN_ED_COCKPIT_COMPARE_PARAMETERS)  (void		  * handle_1, void * handle_2);
typedef void(*PFN_ED_COCKPIT_SET_DRAW_ARGUMENT) (int index, float value);

struct cockpit_param_api {
    PFN_ED_COCKPIT_GET_PARAMETER_HANDLE				 pfn_ed_cockpit_get_parameter_handle;
    PFN_ED_COCKPIT_UPDATE_PARAMETER_WITH_STRING		 pfn_ed_cockpit_update_parameter_with_string;
    PFN_ED_COCKPIT_UPDATE_PARAMETER_WITH_NUMBER		 pfn_ed_cockpit_update_parameter_with_number;
    PFN_ED_COCKPIT_PARAMETER_VALUE_TO_NUMBER		 pfn_ed_cockpit_parameter_value_to_number;
    PFN_ED_COCKPIT_PARAMETER_VALUE_TO_STRING		 pfn_ed_cockpit_parameter_value_to_string;
};



extern "C" {
    DLL_API void ed_fm_set_draw_args(EdDrawArgument * drawargs, size_t size);
    DLL_API double ed_fm_get_param(unsigned index);
    DLL_API void ed_fm_simulate(double dt);
    DLL_API void ed_fm_set_command(int command, float value);
    DLL_API void ed_fm_set_draw_args(EdDrawArgument * drawargs, size_t size);
    DLL_API double ed_fm_get_param(unsigned index);
    DLL_API void ed_fm_add_local_force(double & x, double &y, double &z, double & pos_x, double & pos_y, double & pos_z);
    //DLL_API void ed_fm_add_global_force(double & x, double &y, double &z, double & pos_x, double & pos_y, double & pos_z);
    //DLL_API bool ed_fm_add_local_force_component(double & x, double &y, double &z, double & pos_x, double & pos_y, double & pos_z);
    //DLL_API bool ed_fm_add_global_force_component(double & x, double &y, double &z, double & pos_x, double & pos_y, double & pos_z);

    //DLL_API void ed_fm_add_global_moment(double & x, double &y, double &z);
    DLL_API void ed_fm_add_local_moment(double & x, double &y, double &z);

    //DLL_API bool ed_fm_add_local_moment_component(double & x, double &y, double &z);

    //DLL_API bool ed_fm_add_global_moment_component(double & x, double &y, double &z);

    DLL_API void ed_fm_set_atmosphere(double h,//altitude above sea level
        double t,//current atmosphere temperature , Kelwins
        double a,//speed of sound
        double ro,// atmosphere density
        double p,// atmosphere pressure
        double wind_vx,//components of velocity vector, including turbulence in world coordinate system
        double wind_vy,//components of velocity vector, including turbulence in world coordinate system
        double wind_vz //components of velocity vector, including turbulence in world coordinate system
    );
    DLL_API void ed_fm_set_current_mass_state(double mass,
        double center_of_mass_x,
        double center_of_mass_y,
        double center_of_mass_z,
        double moment_of_inertia_x,
        double moment_of_inertia_y,
        double moment_of_inertia_z
    );
    DLL_API void ed_fm_set_current_state(double ax,//linear acceleration component in world coordinate system
        double ay,//linear acceleration component in world coordinate system
        double az,//linear acceleration component in world coordinate system
        double vx,//linear velocity component in world coordinate system
        double vy,//linear velocity component in world coordinate system
        double vz,//linear velocity component in world coordinate system
        double px,//center of the body position in world coordinate system
        double py,//center of the body position in world coordinate system
        double pz,//center of the body position in world coordinate system
        double omegadotx,//angular accelearation components in world coordinate system
        double omegadoty,//angular accelearation components in world coordinate system
        double omegadotz,//angular accelearation components in world coordinate system
        double omegax,//angular velocity components in world coordinate system
        double omegay,//angular velocity components in world coordinate system
        double omegaz,//angular velocity components in world coordinate system
        double quaternion_x,//orientation quaternion components in world coordinate system
        double quaternion_y,//orientation quaternion components in world coordinate system
        double quaternion_z,//orientation quaternion components in world coordinate system
        double quaternion_w //orientation quaternion components in world coordinate system
    );
    DLL_API void ed_fm_set_current_state_body_axis(double ax,//linear acceleration component in body coordinate system
        double ay,//linear acceleration component in body coordinate system
        double az,//linear acceleration component in body coordinate system
        double vx,//linear velocity component in body coordinate system
        double vy,//linear velocity component in body coordinate system
        double vz,//linear velocity component in body coordinate system
        double wind_vx,//wind linear velocity component in body coordinate system
        double wind_vy,//wind linear velocity component in body coordinate system
        double wind_vz,//wind linear velocity component in body coordinate system

        double omegadotx,//angular accelearation components in body coordinate system
        double omegadoty,//angular accelearation components in body coordinate system
        double omegadotz,//angular accelearation components in body coordinate system
        double omegax,//angular velocity components in body coordinate system
        double omegay,//angular velocity components in body coordinate system
        double omegaz,//angular velocity components in body coordinate system
        double yaw,  //radians
        double pitch,//radians
        double roll, //radians
        double common_angle_of_attack, //AoA radians
        double common_angle_of_slide   //AoS radians
    );
    DLL_API bool ed_fm_change_mass(double & delta_mass,
        double & delta_mass_pos_x,
        double & delta_mass_pos_y,
        double & delta_mass_pos_z,
        double & delta_mass_moment_of_inertia_x,
        double & delta_mass_moment_of_inertia_y,
        double & delta_mass_moment_of_inertia_z
    );
    /*
    set internal fuel volume , init function, called on object creation and for refueling ,
    you should distribute it inside at different fuel tanks
    */
    DLL_API void   ed_fm_set_internal_fuel(double fuel);
    /*
    get internal fuel volume
    */
    DLL_API double ed_fm_get_internal_fuel();
    /*
    set external fuel volume for each payload station , called for weapon init and on reload
    */
    DLL_API void  ed_fm_set_external_fuel(int	 station,
        double fuel,
        double x,
        double y,
        double z);
    /*
    get external fuel volume
    */
    DLL_API double ed_fm_get_external_fuel();

    DLL_API void ed_fm_configure(const char * cfg_path);

    DLL_API void ed_fm_set_plugin_data_install_path(const char * cfg_path);

    /*
    call backs for diffrenrt starting conditions
    */
    //DLL_API void ed_fm_cold_start();
    //DLL_API void ed_fm_hot_start();
    //DLL_API void ed_fm_hot_start_in_air();

    DLL_API void ed_fm_suspension_feedback(int idx, const ed_fm_suspension_info * info);

    DLL_API void ed_fm_set_surface(double		h, //surface height under the center of aircraft
        double		h_obj, //surface height with objects
        unsigned		surface_type,
        double		normal_x, //components of normal vector to surface
        double		normal_y, //components of normal vector to surface
        double		normal_z //components of normal vector to surface
    );

    DLL_API void ed_fm_set_command(int command, float value);
}

class CockpitItem
{
protected:
    void*  Handle;
    std::string Name;
    JSBSim::FGPropertyNode* input_node;
    double Factor;
    double LastVal;
    int c;
public:
    CockpitItem() : Handle(0), Name(), input_node(nullptr), LastVal(-10000000.0), Factor(1.0), c(0) {    }
    CockpitItem(void *handle, FGJSBsim* model, std::string name,  std::string prop, double factor) : Handle(handle), Name(name), Factor(factor)
    {
        input_node = model->PropertyManager->GetNode(prop);
        LastVal = -10000000.0;
        c = 0;
    }
    virtual void Update(cockpit_param_api api, FGJSBsim *model)
    {
        if (!Handle)
        {
            if (!c)
                printf("CockpitItem:: %s has no handle\n", Name.c_str());
            c = 1;
            return;
        }
        if (api.pfn_ed_cockpit_update_parameter_with_number)
        {
            if (input_node != nullptr)
            {
                double val = input_node->getDoubleValue();
                //    if (val != LastVal)
                {
                    //if (!c)
                    //    printf("CockpitItem:: update %s:%s -> %.1f\n", Name.c_str(), Property.c_str(), val*Factor);
                    //c++;
                    api.pfn_ed_cockpit_update_parameter_with_number(Handle, val * Factor);
                    LastVal = val;
                }
                //else if (c) 
                    //c++;
                //if (c > 300)c = 0;
            }
        }
    }
};

class LinearActuator : public CockpitItem
{
    enum class ActuatorType { ActuatorTypeNone, GenevaDrive, LinearDrive} ActuatorType;
public:
    LinearActuator(FGJSBsim* model, void* Handle, const std::string &name,  const std::string &prop, const std::string &type, double factor)
        :CockpitItem(Handle, model, name, prop, factor)
    {
        if (type.c_str() == std::string("GenevaDrive"))
            ActuatorType = ActuatorType::GenevaDrive;
        else if (type.c_str() == std::string("LinearDrive"))
            ActuatorType = ActuatorType::LinearDrive;
        else {
            ActuatorType = ActuatorType::ActuatorTypeNone;
            SG_LOG(SG_FLIGHT, SG_ALERT, "Unknown type for Linear Actuator " << type << " for actuator " << name);
        }
    }
    double getV(double value) {
        double scroll = floor(Factor / 10.0);
        double v1 = floor(value / Factor) * Factor;
        double v2 = floor(value - Factor);
        double v3 = scroll - (v1 - v2);
        double incr = max(0.0, v3);
        return fmod(floor(value / Factor), 10) / 10.0
            + (incr / Factor);
    }
    virtual void Update(cockpit_param_api api, FGJSBsim* model)
    {
        if (input_node) {
            double value = input_node->getDoubleValue();
            
            // now convert
            switch (ActuatorType) {
            case ActuatorType::GenevaDrive:
                value = getV(value);
                break;

            case ActuatorType::LinearDrive:
                value = fmod(value, 1000) / 10.0;
                break;
            }
            if (value != LastVal) {
                api.pfn_ed_cockpit_update_parameter_with_number(Handle, value);
                LastVal = value;
            }
        }
    }
};

class Cockpit
{
protected:
    cockpit_param_api Api;
    std::vector<CockpitItem *> items;
public:
    void Connect(cockpit_param_api api)
    {
        Api = api;
    }
    void AddItem(CockpitItem *ci)
    {
        items.push_back(ci);
    }
    void *FindHandle(std::string param)
    {
        if (Api.pfn_ed_cockpit_get_parameter_handle)
            return Api.pfn_ed_cockpit_get_parameter_handle(param.c_str());
        else
            printf("Cannot locate %s - no connection to cockpit\n", param.c_str());
        return 0;
    }
    void AddItem(FGJSBsim* model, std::string param, std::string prop, double factor = 1.0)
    {
        void *handle = FindHandle(param);

        if (handle)
        {
            printf("CockpitItem:: %s -> %s (%.1f) == $%x\n", param.c_str(), prop.c_str(), factor, handle);
            items.push_back(new CockpitItem(handle, model, param, prop, factor));
        }
        else
            printf("Cockpit:: failed to locate parameter_name %s\n", param.c_str());
    }
    void Update(FGJSBsim *model)
    {
        for (auto&& item : items)
        {
            item->Update(Api, model);
        }
    }
};
const std::map<string, int>::value_type enumMap[] = {
    map<string, int>::value_type("IN_BODY_SPACE", 0),
    map<string, int>::value_type("IN_WORLD_SPACE", 1),
    map<string, int>::value_type("ED_FM_ENGINE_0_RPM", 0),
    map<string, int>::value_type("ED_FM_ENGINE_0_RELATED_RPM", 1),
    map<string, int>::value_type("ED_FM_ENGINE_0_CORE_RPM", 2),
    map<string, int>::value_type("ED_FM_ENGINE_0_CORE_RELATED_RPM", 3),
    map<string, int>::value_type("ED_FM_ENGINE_0_THRUST", 4),
    map<string, int>::value_type("ED_FM_ENGINE_0_RELATED_THRUST", 5),
    map<string, int>::value_type("ED_FM_ENGINE_0_CORE_THRUST", 6),
    map<string, int>::value_type("ED_FM_ENGINE_0_CORE_RELATED_THRUST", 7),
    map<string, int>::value_type("ED_FM_PROPELLER_0_RPM", 8),
    map<string, int>::value_type("ED_FM_PROPELLER_0_PITCH", 9),
    map<string, int>::value_type("ED_FM_PROPELLER_0_TILT", 10),
    map<string, int>::value_type("ED_FM_PROPELLER_0_INTEGRITY_FACTOR", 11),
    map<string, int>::value_type("ED_FM_ENGINE_0_TEMPERATURE", 12),
    map<string, int>::value_type("ED_FM_ENGINE_0_OIL_PRESSURE", 13),
    map<string, int>::value_type("ED_FM_ENGINE_0_FUEL_FLOW", 14),
    map<string, int>::value_type("ED_FM_ENGINE_0_COMBUSTION", 15),
    map<string, int>::value_type("ED_FM_PISTON_ENGINE_0_MANIFOLD_PRESSURE", 16),
    map<string, int>::value_type("ED_FM_ENGINE_0_STARTER_RELATED_RPM", 17),
    map<string, int>::value_type("ED_FM_ENGINE_0_STARTER_RELATED_TORQUE", 18),
    map<string, int>::value_type("ED_FM_ENGINE_0_STARTER_SPIN_DOWN_RELATED_RPM", 19),
    map<string, int>::value_type("ED_FM_ENGINE_0_STARTER_SPIN_DOWN_RELATED_TORQUE", 20),
    map<string, int>::value_type("ED_FM_ENGINE_0_STARTER_CLUTCH_ENGAGED_RELATED_RPM", 21),
    map<string, int>::value_type("ED_FM_ENGINE_0_STARTER_CLUTCH_ENGAGED_RELATED_TORQUE", 22),
    map<string, int>::value_type("ED_FM_ENGINE_0_TORQUE", 23),
    map<string, int>::value_type("ED_FM_ENGINE_0_RELATIVE_TORQUE", 24),
    map<string, int>::value_type("ED_FM_ENGINE_1_RPM", 100),
    map<string, int>::value_type("ED_FM_ENGINE_1_RELATED_RPM", 101),
    map<string, int>::value_type("ED_FM_ENGINE_1_CORE_RPM", 102),
    map<string, int>::value_type("ED_FM_ENGINE_1_CORE_RELATED_RPM", 103),
    map<string, int>::value_type("ED_FM_ENGINE_1_THRUST", 104),
    map<string, int>::value_type("ED_FM_ENGINE_1_RELATED_THRUST", 105),
    map<string, int>::value_type("ED_FM_ENGINE_1_CORE_THRUST", 106),
    map<string, int>::value_type("ED_FM_ENGINE_1_CORE_RELATED_THRUST", 107),
    map<string, int>::value_type("ED_FM_PROPELLER_1_RPM", 108),
    map<string, int>::value_type("ED_FM_PROPELLER_1_PITCH", 109),
    map<string, int>::value_type("ED_FM_PROPELLER_1_TILT", 110),
    map<string, int>::value_type("ED_FM_PROPELLER_1_INTEGRITY_FACTOR", 111),
    map<string, int>::value_type("ED_FM_ENGINE_1_TEMPERATURE", 112),
    map<string, int>::value_type("ED_FM_ENGINE_1_OIL_PRESSURE", 113),
    map<string, int>::value_type("ED_FM_ENGINE_1_FUEL_FLOW", 114),
    map<string, int>::value_type("ED_FM_ENGINE_1_COMBUSTION", 115),
    map<string, int>::value_type("ED_FM_PISTON_ENGINE_1_MANIFOLD_PRESSURE", 116),
    map<string, int>::value_type("ED_FM_ENGINE_1_STARTER_RELATED_RPM", 117),
    map<string, int>::value_type("ED_FM_ENGINE_1_STARTER_RELATED_TORQUE", 118),
    map<string, int>::value_type("ED_FM_ENGINE_1_STARTER_SPIN_DOWN_RELATED_RPM", 119),
    map<string, int>::value_type("ED_FM_ENGINE_1_STARTER_SPIN_DOWN_RELATED_TORQUE", 120),
    map<string, int>::value_type("ED_FM_ENGINE_1_STARTER_CLUTCH_ENGAGED_RELATED_RPM", 121),
    map<string, int>::value_type("ED_FM_ENGINE_1_STARTER_CLUTCH_ENGAGED_RELATED_TORQUE", 122),
    map<string, int>::value_type("ED_FM_ENGINE_1_TORQUE", 123),
    map<string, int>::value_type("ED_FM_ENGINE_1_RELATIVE_TORQUE", 124),
    map<string, int>::value_type("ED_FM_ENGINE_2_RPM", 200),
    map<string, int>::value_type("ED_FM_ENGINE_2_RELATED_RPM", 201),
    map<string, int>::value_type("ED_FM_ENGINE_2_CORE_RPM", 202),
    map<string, int>::value_type("ED_FM_ENGINE_2_CORE_RELATED_RPM", 203),
    map<string, int>::value_type("ED_FM_ENGINE_2_THRUST", 204),
    map<string, int>::value_type("ED_FM_ENGINE_2_RELATED_THRUST", 205),
    map<string, int>::value_type("ED_FM_ENGINE_2_CORE_THRUST", 206),
    map<string, int>::value_type("ED_FM_ENGINE_2_CORE_RELATED_THRUST", 207),
    map<string, int>::value_type("ED_FM_PROPELLER_2_RPM", 208),
    map<string, int>::value_type("ED_FM_PROPELLER_2_PITCH", 209),
    map<string, int>::value_type("ED_FM_PROPELLER_2_TILT", 210),
    map<string, int>::value_type("ED_FM_PROPELLER_2_INTEGRITY_FACTOR", 211),
    map<string, int>::value_type("ED_FM_ENGINE_2_TEMPERATURE", 212),
    map<string, int>::value_type("ED_FM_ENGINE_2_OIL_PRESSURE", 213),
    map<string, int>::value_type("ED_FM_ENGINE_2_FUEL_FLOW", 214),
    map<string, int>::value_type("ED_FM_ENGINE_2_COMBUSTION", 215),
    map<string, int>::value_type("ED_FM_PISTON_ENGINE_2_MANIFOLD_PRESSURE", 216),
    map<string, int>::value_type("ED_FM_ENGINE_2_STARTER_RELATED_RPM", 217),
    map<string, int>::value_type("ED_FM_ENGINE_2_STARTER_RELATED_TORQUE", 218),
    map<string, int>::value_type("ED_FM_ENGINE_2_STARTER_SPIN_DOWN_RELATED_RPM", 219),
    map<string, int>::value_type("ED_FM_ENGINE_2_STARTER_SPIN_DOWN_RELATED_TORQUE", 220),
    map<string, int>::value_type("ED_FM_ENGINE_2_STARTER_CLUTCH_ENGAGED_RELATED_RPM", 221),
    map<string, int>::value_type("ED_FM_ENGINE_2_STARTER_CLUTCH_ENGAGED_RELATED_TORQUE", 222),
    map<string, int>::value_type("ED_FM_ENGINE_2_TORQUE", 223),
    map<string, int>::value_type("ED_FM_ENGINE_2_RELATIVE_TORQUE", 224),
    map<string, int>::value_type("ED_FM_ENGINE_3_RPM", 300),
    map<string, int>::value_type("ED_FM_END_ENGINE_BLOCK", 2000),
    map<string, int>::value_type("ED_FM_SUSPENSION_0_RELATIVE_BRAKE_MOMENT", 2001),
    map<string, int>::value_type("ED_FM_SUSPENSION_0_GEAR_POST_STATE", 2002),
    map<string, int>::value_type("ED_FM_SUSPENSION_0_UP_LOCK", 2003),
    map<string, int>::value_type("ED_FM_SUSPENSION_0_DOWN_LOCK", 2004),
    map<string, int>::value_type("ED_FM_SUSPENSION_0_WHEEL_YAW", 2005),
    map<string, int>::value_type("ED_FM_SUSPENSION_0_WHEEL_SELF_ATTITUDE", 2006),
    map<string, int>::value_type("ED_FM_SUSPENSION_1_RELATIVE_BRAKE_MOMENT", 2011),
    map<string, int>::value_type("ED_FM_SUSPENSION_1_GEAR_POST_STATE", 2012),
    map<string, int>::value_type("ED_FM_SUSPENSION_1_UP_LOCK", 2013),
    map<string, int>::value_type("ED_FM_SUSPENSION_1_DOWN_LOCK", 2014),
    map<string, int>::value_type("ED_FM_SUSPENSION_1_WHEEL_YAW", 2015),
    map<string, int>::value_type("ED_FM_SUSPENSION_1_WHEEL_SELF_ATTITUDE", 2016),
    map<string, int>::value_type("ED_FM_SUSPENSION_2_RELATIVE_BRAKE_MOMENT", 2021),
    map<string, int>::value_type("ED_FM_SUSPENSION_2_GEAR_POST_STATE", 2022),
    map<string, int>::value_type("ED_FM_SUSPENSION_2_UP_LOCK", 2023),
    map<string, int>::value_type("ED_FM_SUSPENSION_2_DOWN_LOCK", 2024),
    map<string, int>::value_type("ED_FM_SUSPENSION_2_WHEEL_YAW", 2025),
    map<string, int>::value_type("ED_FM_SUSPENSION_2_WHEEL_SELF_ATTITUDE", 2026),
    map<string, int>::value_type("ED_FM_SUSPENSION_3_RELATIVE_BRAKE_MOMENT", 2031),
    map<string, int>::value_type("ED_FM_SUSPENSION_4_RELATIVE_BRAKE_MOMENT", 2041),
    map<string, int>::value_type("ED_FM_SUSPENSION_5_RELATIVE_BRAKE_MOMENT", 2051),
    map<string, int>::value_type("ED_FM_SUSPENSION_6_RELATIVE_BRAKE_MOMENT", 2061),
    map<string, int>::value_type("ED_FM_SUSPENSION_7_RELATIVE_BRAKE_MOMENT", 2071),
    map<string, int>::value_type("ED_FM_SUSPENSION_8_RELATIVE_BRAKE_MOMENT", 2081),
    map<string, int>::value_type("ED_FM_SUSPENSION_9_RELATIVE_BRAKE_MOMENT", 2091),
    map<string, int>::value_type("ED_FM_SUSPENSION_10_RELATIVE_BRAKE_MOMENT", 2101),
    map<string, int>::value_type("ED_FM_OXYGEN_SUPPLY", 2102),
    map<string, int>::value_type("ED_FM_FLOW_VELOCITY", 2103),
    map<string, int>::value_type("ED_FM_CAN_ACCEPT_FUEL_FROM_TANKER", 2104),
    map<string, int>::value_type("ED_FM_FUEL_FUEL_TANK_GROUP_0_LEFT", 2105),
    map<string, int>::value_type("ED_FM_FUEL_FUEL_TANK_GROUP_0_RIGHT", 2106),
    map<string, int>::value_type("ED_FM_FUEL_FUEL_TANK_GROUP_1_LEFT", 2107),
    map<string, int>::value_type("ED_FM_FUEL_FUEL_TANK_GROUP_1_RIGHT", 2108),
    map<string, int>::value_type("ED_FM_FUEL_FUEL_TANK_GROUP_2_LEFT", 2109),
    map<string, int>::value_type("ED_FM_FUEL_FUEL_TANK_GROUP_2_RIGHT", 2110),
    map<string, int>::value_type("ED_FM_FUEL_FUEL_TANK_GROUP_3_LEFT", 2111),
    map<string, int>::value_type("ED_FM_FUEL_FUEL_TANK_GROUP_3_RIGHT", 2112),
    map<string, int>::value_type("ED_FM_FUEL_FUEL_TANK_GROUP_4_LEFT", 2113),
    map<string, int>::value_type("ED_FM_FUEL_FUEL_TANK_GROUP_4_RIGHT", 2114),
    map<string, int>::value_type("ED_FM_FUEL_FUEL_TANK_GROUP_5_LEFT", 2115),
    map<string, int>::value_type("ED_FM_FUEL_FUEL_TANK_GROUP_5_RIGHT", 2116),
    map<string, int>::value_type("ED_FM_FUEL_FUEL_TANK_GROUP_6_LEFT", 2117),
    map<string, int>::value_type("ED_FM_FUEL_FUEL_TANK_GROUP_6_RIGHT", 2118),
    map<string, int>::value_type("ED_FM_FUEL_INTERNAL_FUEL", 2119),
    map<string, int>::value_type("ED_FM_FUEL_TOTAL_FUEL", 2120),
    map<string, int>::value_type("ED_FM_FUEL_LOW_SIGNAL", 2121),
    map<string, int>::value_type("ED_FM_ANTI_SKID_ENABLE", 2122),
    map<string, int>::value_type("ED_FM_STICK_FORCE_CENTRAL_PITCH", 2123),
    map<string, int>::value_type("ED_FM_STICK_FORCE_FACTOR_PITCH", 2124),
    map<string, int>::value_type("ED_FM_STICK_FORCE_SHAKE_AMPLITUDE_PITCH", 2125),
    map<string, int>::value_type("ED_FM_STICK_FORCE_SHAKE_FREQUENCY_PITCH", 2126),
    map<string, int>::value_type("ED_FM_STICK_FORCE_CENTRAL_ROLL", 2127),
    map<string, int>::value_type("ED_FM_STICK_FORCE_FACTOR_ROLL", 2128),
    map<string, int>::value_type("ED_FM_STICK_FORCE_SHAKE_AMPLITUDE_ROLL", 2129),
    map<string, int>::value_type("ED_FM_STICK_FORCE_SHAKE_FREQUENCY_ROLL", 2130),
    map<string, int>::value_type("ED_FM_COCKPIT_PRESSURIZATION_OVER_EXTERNAL", 2131),
    map<string, int>::value_type("ED_FM_COCKPIT_ALTIMETER_PRESSURE_SETTING_MM_HG", 2132),
    map<string, int>::value_type("ED_FM_FC3_RESERVED_SPACE", 10000),
    map<string, int>::value_type("ED_FM_FC3_GEAR_HANDLE_POS", 10001),
    map<string, int>::value_type("ED_FM_FC3_FLAPS_HANDLE_POS", 10002),
    map<string, int>::value_type("ED_FM_FC3_SPEED_BRAKE_HANDLE_POS", 10003),
    map<string, int>::value_type("ED_FM_FC3_STICK_PITCH", 10004),
    map<string, int>::value_type("ED_FM_FC3_STICK_ROLL", 10005),
    map<string, int>::value_type("ED_FM_FC3_RUDDER_PEDALS", 10006),
    map<string, int>::value_type("ED_FM_FC3_THROTTLE_LEFT", 10007),
    map<string, int>::value_type("ED_FM_FC3_THROTTLE_RIGHT", 10008),
    map<string, int>::value_type("ED_FM_FC3_CAS_FAILURE_PITCH_CHANNEL", 10009),
    map<string, int>::value_type("ED_FM_FC3_CAS_FAILURE_ROLL_CHANNEL", 10010),
    map<string, int>::value_type("ED_FM_FC3_CAS_FAILURE_YAW_CHANNEL", 10011),
    map<string, int>::value_type("ED_FM_FC3_AUTOPILOT_STATUS", 10012),
    map<string, int>::value_type("ED_FM_FC3_AUTOPILOT_FAILURE_ATTITUDE_STABILIZATION", 10013),
    map<string, int>::value_type("ED_FM_FC3_STICK_PITCH_LIMITER", 10014),
    map<string, int>::value_type("ED_FM_FC3_STICK_ROLL_L_LIMITER", 10015),
    map<string, int>::value_type("ED_FM_FC3_PEDAL_L_LIMITER", 10016),
    map<string, int>::value_type("ED_FM_FC3_PEDAL_R_LIMITER", 10017),
    map<string, int>::value_type("ED_FM_FC3_BREAKE_CHUTE_STATUS", 10018),
    map<string, int>::value_type("ED_FM_FC3_BREAKE_CHUTE_VALUE", 10019),
    map<string, int>::value_type("ED_FM_FC3_WHEEL_BRAKE_LEFT", 10020),
    map<string, int>::value_type("ED_FM_FC3_WHEEL_BRAKE_RIGHT", 10021),
    map<string, int>::value_type("ED_FM_FC3_WHEEL_BRAKE_COMMAND_LEFT", 10022),
    map<string, int>::value_type("ED_FM_FC3_WHEEL_BRAKE_COMMAND_RIGHT", 10023),
    map<string, int>::value_type("ED_FM_FC3_AR_DOOR_PROBE_HANDLE_POS", 10024),
    map<string, int>::value_type("ED_FM_FC3_RESERVED_SPACE_END", 11000),
    map<string, int>::value_type("ED_FM_EVENT_INVALID", 0),
    map<string, int>::value_type("ED_FM_EVENT_FAILURE", 1),
    map<string, int>::value_type("ED_FM_EVENT_STRUCTURE_DAMAGE", 2),
    map<string, int>::value_type("ED_FM_EVENT_FIqRE", 3),
    map<string, int>::value_type("ED_FM_EVENT_CARRIER_CATAPULT", 4),
};
class DCS_interface
{
public:
    Cockpit cockpit;
    DCS_interface();
    ~DCS_interface();
    void initialize(FGJSBsim *model);
    void simulate(double dt);
};
