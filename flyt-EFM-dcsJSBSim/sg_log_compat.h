#pragma once

// Maps the legacy SimGear SG_LOG macro onto JSBSim's FGLogging. The bridge logs
// through SG_LOG in many places, while upstream JSBSim provides FGLogging. The
// SG_LOG channel argument has no FGLogging equivalent and is ignored; the
// priority maps to a LogLevel.

// windows.h defines an ERROR macro that clashes with JSBSim's LogLevel::ERROR enumerator.
#ifdef ERROR
#undef ERROR
#endif
#include <input_output/FGLog.h>

// SimGear priorities mapped to JSBSim log levels.
#define SG_BULK  ::JSBSim::LogLevel::BULK
#define SG_DEBUG ::JSBSim::LogLevel::DEBUG
#define SG_INFO  ::JSBSim::LogLevel::INFO
#define SG_WARN  ::JSBSim::LogLevel::WARN
#define SG_ALERT ::JSBSim::LogLevel::ERROR
#define SG_POPUP ::JSBSim::LogLevel::INFO

// SG_LOG(channel, priority, message) where message is a streaming expression.
#define SG_LOG(channel, priority, message) \
    do {                                   \
        ::JSBSim::FGLogging _sg_log(priority); \
        _sg_log << message << "\n";        \
    } while (0)
