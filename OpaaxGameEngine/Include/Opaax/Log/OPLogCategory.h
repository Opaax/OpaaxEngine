#pragma once
#include "Opaax/OpaaxCoreMacros.h"

/**
 * @enum ELogCategory
 *
 * Represents different categories of logging levels.
 * These categories can be used to filter or identify the nature of log messages.
 *
 * @var ELC_Info
 * Log category for informational messages.
 * Typically used for general operational logs or non-critical information.
 *
 * @var ELC_Warning
 * Log category for warning messages.
 * Signifies a situation that is unexpected or may require attention but is not critical.
 *
 * @var ELC_Error
 * Log category for error messages.
 * Indicates significant issues that may prevent normal operation or require immediate attention.
 *
 * @var ELC_Debug
 * Log category for debug messages.
 * Primarily used for development and debugging purposes to track internal state and application flow.
 *
 * @var ELC_Verbose
 * Log category for verbose messages.
 * Used for detailed logging that may include exhaustive operational details.
 */
enum struct ELogCategory
{
    ELC_Info = 0,
    ELC_Warning = 1,
    ELC_Error = 2,
    ELC_Debug = 3,
    ELC_Verbose = 4
};
