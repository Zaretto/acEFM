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

class DCS_interface
{
public:
    Cockpit cockpit;
    DCS_interface();
    ~DCS_interface();
    void initialize(FGJSBsim *model);
    void simulate(double dt);
};
