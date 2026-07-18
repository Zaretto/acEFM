// TestPlane.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "../flyt-EFM-dcsJSBSim/DCS_interface.h"
#include "CDirectInputJoystick.h"
#include "instruments.h"
#include <map>
#include <string>
#include <string.h>
#include <stdlib.h>
using namespace std;

// Keeps the bridge-callback recorder off in TestPlane regardless of the
// acefm/debug-record property, so a replay can never truncate the recording it
// is reading (the recorder writes to the fixed path the replay reads from).
extern "C" __declspec(dllimport) void acefm_suppress_debug_record(void);

    int main(int argc, char **argv)
{
    
    try {
        std::string userPath;
    if (argc < 2) {
        fprintf(stderr, "full path required as first argument");// userPath = "C:\\Users\\Richard\\Saved Games\\DCS.openbeta\\Mods\\aircraft\\F-15T";
        return 0;
    } 
    else
        userPath = argv[1];
    // The script-running modes (jsbsim, test, test-fg, bridge-test) moved to
    // run_validation.py, which invokes JSBSim.exe directly. Reject them here so a
    // stale caller fails loudly instead of being treated as an aircraft path, but
    // first echo the equivalent JSBSim.exe command line so the path conventions
    // (which are easy to forget) don't have to be remembered by hand.
    if (userPath == "jsbsim" || userPath == "test" || userPath == "test-fg"
        || userPath == "bridge-test") {
        // Path conventions kept in sync with run_validation.py's _PATH_CONVENTIONS.
        // jsbsim mode passed its remaining args straight through to JSBSim.
        const char* conv = nullptr;
        if (userPath == "test")
            conv = "--root=. --aircraft-path=EFM --engine-path=EFM/Engines "
                   "--systems-path=EFM/Systems --init-path=autotest/init";
        else if (userPath == "test-fg")
            conv = "--root=. --aircraft-path=. --engine-path=Engines "
                   "--systems-path=Systems --init-path=autotest/init";
        else if (userPath == "bridge-test")
            conv = "--root=. --aircraft-path=EFM --engine-path=EFM/Engines "
                   "--systems-path=EFM/Systems --init-path=autotest/init "
                   "--property=simulation/models/propagate/enabled=0 "
                   "--property=simulation/models/groundreactions/enabled=0";

        fprintf(stderr,
            "TestPlane: '%s' mode has moved to run_validation.py, which runs "
            "JSBSim.exe directly. TestPlane handles only the EFM modes (check, "
            "replay, and the live harness).\n\n",
            userPath.c_str());

        if (userPath == "jsbsim") {
            // jsbsim was a thin pass-through: everything after the mode word.
            fprintf(stderr, "Equivalent JSBSim.exe command (run from the aircraft mod dir):\n  JSBSim.exe");
            for (int i = 2; i < argc; ++i)
                fprintf(stderr, " %s", argv[i]);
            fprintf(stderr, "\n");
        } else {
            const char* script = (argc > 2) ? argv[2] : "<script.xml>";
            const char* mode = (userPath == "test") ? "dcs"
                             : (userPath == "test-fg") ? "fg" : "bridge";
            fprintf(stderr, "Equivalent JSBSim.exe command (run from the aircraft mod dir):\n  JSBSim.exe --script=%s %s", script, conv);
            for (int i = 3; i < argc; ++i)
                fprintf(stderr, " %s", argv[i]);
            fprintf(stderr,
                "\n\nOr via the runner:\n  python autotest/run_validation.py --aircraft <AircraftMod> --mode %s\n",
                mode);
        }
        return 2;
    }
    int frames_to_run = -1;
    if (userPath == "check") {
        acefm_suppress_debug_record();
        frames_to_run = 20;
        userPath = argv[2];
    }
    if (userPath == "replay") {
        acefm_suppress_debug_record();
        // Replay a recorded bridge callback log through the live bridge.
        // Usage: TestPlane.exe replay <path-to-dcs-bridge-log.csv>
        // The bridge always uses Run(dcsModels) exactly as it does in live DCS.
        if (argc < 3) {
            fprintf(stderr, "replay: log file path required\n");
            return 1;
        }
        if (!::getenv("JSBSIM_DEBUG")) ::putenv("JSBSIM_DEBUG=0");
        ed_fm_set_plugin_data_install_path(".");
        FILE* fp = fopen(argv[2], "r");
        if (!fp) { fprintf(stderr, "replay: cannot open %s\n", argv[2]); return 1; }
        char linebuf[8192];
        long long line_no = 0, n_sim = 0;
        while (fgets(linebuf, sizeof(linebuf), fp)) {
            ++line_no;
            if (linebuf[0] == '#' || linebuf[0] == '\n' || linebuf[0] == '\r' || linebuf[0] == 0)
                continue;
            char* save = nullptr;
            char* tok  = strtok_s(linebuf, ",\r\n", &save);
            if (!tok) continue;
            char* method = strtok_s(nullptr, ",\r\n", &save);
            if (!method) continue;
            double p[24] = {0};
            int np = 0;
            while (np < 24) {
                char* v = strtok_s(nullptr, ",\r\n", &save);
                if (!v) break;
                p[np++] = atof(v);
            }
            if      (strcmp(method, "ATM") == 0 && np >= 8) {
                ed_fm_set_atmosphere(p[0],p[1],p[2],p[3],p[4],p[5],p[6],p[7]);
            } else if (strcmp(method, "STATE") == 0 && np >= 19) {
                ed_fm_set_current_state(
                    p[0],p[1],p[2], p[3],p[4],p[5], p[6],p[7],p[8],
                    p[9],p[10],p[11], p[12],p[13],p[14],
                    p[15],p[16],p[17],p[18]);
            } else if (strcmp(method, "STATE_BODY") == 0 && np >= 20) {
                ed_fm_set_current_state_body_axis(
                    p[0],p[1],p[2], p[3],p[4],p[5], p[6],p[7],p[8],
                    p[9],p[10],p[11], p[12],p[13],p[14],
                    p[15],p[16],p[17], p[18], p[19]);
            } else if (strcmp(method, "CMD") == 0 && np >= 2) {
                ed_fm_set_command((int)p[0], (float)p[1]);
            } else if (strcmp(method, "SIM") == 0 && np >= 1) {
                ed_fm_simulate(p[0]);
                ++n_sim;
            }
        }
        fclose(fp);
        fprintf(stderr, "replay: %lld lines, %lld SIM ticks\n", line_no, n_sim);
        return 0;
    }
    SetCurrentDirectoryA(userPath.c_str());

    CDirectInputJoystick dij;
    dij.InitDI(nullptr, GetModuleHandle(NULL));
    int dt = 100; //ms
    map <string, map <string, double> > convert;
    convert["M"]["FT"] = 3.2808399;
    convert["FT"]["M"] = 1.0 / convert["M"]["FT"];
    ed_fm_set_plugin_data_install_path(userPath.c_str());
    EdDrawArgument drawargs[1000];

    InstrumentsInit(GetModuleHandle(NULL));

        while (true) {
            double yaw = 0.1;
            double pitch = 0.2;
            double roll = 0.3;
            ed_fm_set_atmosphere(100, 240, 320, 1.023, 0.2, 0.3, 0.4, 0.5);
            ed_fm_set_current_state_body_axis(0, 0, 0, 573, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, yaw, pitch, roll, 0.5, 0.02);
            ed_fm_simulate(dt / 1000.0);
            ed_fm_set_draw_args(&drawargs[0], 1000);

            dij.ProcessJoy();
            InstrumentsUpdate();
            if (frames_to_run > 0 && --frames_to_run == 0)
                break;
            else
                Sleep(dt);
        }

    InstrumentsShutdown();
    } catch (std::runtime_error& be) {
        printf("Exception %s\n", be.what());
    
    }
    catch(const char* e)
    {
        printf("Exception %s\n", e);
    }
    catch (const std::string& e)
    {
        printf("Exception: %s\n", e.c_str());
    } catch (...)
    {
        printf("Exception");
    }
    
    return 0;
}

//}
//catch (std::string ex)
//{
//    MessageBoxA(0, ex.c_str(), "JSBSimEFM: Exception", 0);
//    exit(0);
//}
//catch (const char* ex)
//{
//    MessageBoxA(0, ex, "JSBSimEFM: Exception", 0);
//    exit(0);
//}
//catch (...)
//{
//    MessageBoxA(0, "Unknown failure", "JSBSimEFM: Exception", 0);
//    exit(0);
//}
