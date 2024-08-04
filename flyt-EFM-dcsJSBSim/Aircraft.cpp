#include "stdafx.h"
#include "Aircraft.h"
#include "DCS_interface.h"
#include "JSBSim_interface.h"


#include <string>
#include <map>
#include <string>
using namespace std;



namespace Aircraft {
    double airspeed_arg = 0;
    double dt = 0;
    unsigned long elapsed = 0;
}
static FGJSBsim *model = 0;
static DCS_interface *dcs_i = 0;
FGJSBsim *get_model()
{
    if (!model)
    {
        map <string, map <string, double> > convert;
        convert["M"]["FT"] = 3.2808399;
        convert["FT"]["M"] = 1.0 / convert["M"]["FT"];
        model = new FGJSBsim(0);
        model->fgSetBool("/sim/presets/running", true);
        //model->altitude->setDoubleValue(10000);
        model->init();
        dcs_i = new DCS_interface();
        dcs_i->initialize(model);
    }
    return model;

}
void ed_fm_set_draw_args_v2(float* drawargs, size_t size)
{
    printf("ed_fm_set_draw_args_v2 %x %d\n", drawargs, size);
}

void ed_fm_set_draw_args(EdDrawArgument* drawargs, size_t size)
{
    FGJSBsim* model = get_model();
    dcs_i->drawArguments.Update(model, drawargs, size);
    return;
}


void ed_fm_simulate(double dt) {
    //    FGJSBsim *model = get_model();
    get_model()->update(dt);
    dcs_i->simulate(dt);
    //printf("N1 %4.0f alt %5.0f VC %4.0f angles(%5f,%5f) alpha %3.2f beta %3.2f F(%.0f,%.0f,%.0f) M(%.2f,%.2f,%.2f) qb %.4f, el %.1f al %.1f\n",
    //    model->fgGetDouble("/fdm/jsbsim/propulsion/engine/n1", -1),
    //    model->fgGetDouble("/fdm/jsbsim/position/altitude-agl-ft", -1),
    //    model->fgGetDouble("/fdm/jsbsim/velocities/vc-kts", -1),
    //    model->fgGetDouble("/fdm/jsbsim/attitude/theta-deg", -1),
    //    model->fgGetDouble("/fdm/jsbsim/attitude/roll-rad", -1) * RADTODEG,
    //    model->fgGetDouble("/fdm/jsbsim/aero/alpha-deg", -1),
    //    model->fgGetDouble("/fdm/jsbsim/aero/beta-deg", -1),
    //    model->fgGetDouble("/fdm/jsbsim/forces/fbx-aero-lbs"),
    //    model->fgGetDouble("/fdm/jsbsim/forces/fbz-aero-lbs"),
    //    model->fgGetDouble("/fdm/jsbsim/forces/fby-aero-lbs"),
    //    model->fgGetDouble("/fdm/jsbsim/moments/l-total-lbsft"),
    //    model->fgGetDouble("/fdm/jsbsim/moments/m-total-lbsft"),
    //    model->fgGetDouble("/fdm/jsbsim/moments/n-total-lbsft"),
    //    model->fgGetDouble("/fdm/jsbsim/aero/qbar-psf"),
    //    model->fgGetDouble("/fdm/jsbsim/fcs/elevator-pos-deg"),
    //    model->fgGetDouble("/fdm/jsbsim/fcs/aileron-pos-deg")
    //);
   // Aircraft::elapsed += (unsigned long)(dt / 0.006);
    //    double ai = model->fgGetDouble("/fdm/jsbsim/velocities/vc-kts")/1000.0;
    //    cockpit_param.pfn_ed_cockpit_update_parameter_with_number(airspeed_handle, ai);

}
