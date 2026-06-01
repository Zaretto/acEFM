#pragma once

#include "targetver.h"

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
#define NOMINMAX
// Windows Header Files:
#include <windows.h>

// windows.h defines an ERROR macro that clashes with JSBSim's LogLevel::ERROR enumerator.
#ifdef ERROR
#undef ERROR
#endif

#include <iostream>

#include "sg_log_compat.h"


// TODO: reference additional headers your program requires here
