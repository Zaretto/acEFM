// TestPlane.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "../flyt-EFM-dcsJSBSim/DCS_interface.h"
#include "CDirectInputJoystick.h"
#include <map>
#include <string>
using namespace std;
int main()
{
    wprintf(L"Richard's DCS EFM test harness\n");

    CDirectInputJoystick dij;
    dij.InitDI(nullptr, GetModuleHandle(NULL));
    int dt = 100; //ms 
    map <string, map <string, double> > convert;
    convert["M"]["FT"] = 3.2808399;
    convert["FT"]["M"] = 1.0 / convert["M"]["FT"];
    ed_fm_set_plugin_data_install_path("C:\\Users\\Richard\\Saved Games\\DCS.openbeta\\Mods\\aircraft\\F-15T");
    while (true)
    {
        double yaw = 0.1;
        double pitch = 0.2;
        double roll = 0.3;
        ed_fm_set_atmosphere(100, 240, 320, 1.023, 0.2, 0.3, 0.4, 0.5);
        ed_fm_set_current_state_body_axis(0, 0, 0, 573, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, yaw, pitch, roll, 0.5, 0.02);
        ed_fm_simulate(dt / 1000.0);
        dij.ProcessJoy();
        Sleep(dt);
    }
    return 0;
}