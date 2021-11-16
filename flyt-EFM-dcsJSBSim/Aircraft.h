#pragma once

//
//#include "Utility/Utility.h"
//#include "Systems/Systems.h"
#include "DCS-API/include/FM/wHumanCustomPhysicsAPI.h"
//#include "Cockpit/Cockpit.h"

void initialize();


namespace Aircraft {
    extern double airspeed_arg;

    extern double dt;
    extern unsigned long elapsed;
}
