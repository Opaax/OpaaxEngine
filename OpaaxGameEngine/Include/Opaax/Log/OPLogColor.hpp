#pragma once
#include <array>
#include "../OpaaxTypes.h"

/**
 * @enum ELogColor
 *
 * Enumerates various color codes used for logging purposes. Each color
 * is associated with a unique integer value to represent different log
 * colors and provides a way to reset or specify text colors for output.
 *
 * @note The colors correspond to commonly used text/terminal color codes.
 */
enum struct ELogColor : Int8
{
    COLOR_LOG_RESETALL = 0,
    COLOR_LOG_BLACK = 1,
    COLOR_LOG_RED = 2,
    COLOR_LOG_GREEN = 3,
    COLOR_LOG_YELLOW = 4,
    COLOR_LOG_BLUE = 5,
    COLOR_LOG_MAGENTA = 6,
    COLOR_LOG_CYAN = 7,
    COLOR_LOG_LIGHTGRAY = 8,
    COLOR_LOG_DARKGRAY = 9,
    COLOR_LOG_LIGHTRED = 10,
    COLOR_LOG_LIGHTGREEN = 11,
    COLOR_LOG_LIGHTYELLOW = 12,
    COLOR_LOG_LIGHTBLUE = 13,
    COLOR_LOG_LIGHTMAGENTA = 14,
    COLOR_LOG_LIGHTCYAN = 15,
    COLOR_LOG_WHITE = 16,
};

/**
 * @var GAllLogColor
 *
 * A collection of ANSI escape sequences representing various terminal text color codes.
 * This array contains 17 predefined color codes that can be used for formatting log outputs
 * in different colors. The sequence "\033[0m" is used to reset the text formatting to the default.
 *
 * Each color corresponds to a specific terminal color code:
 * - Position 0: Reset
 * - Position 1: Black
 * - Position 2: Red
 * - Position 3: Green
 * - Position 4: Yellow
 * - Position 5: Blue
 * - Position 6: Magenta
 * - Position 7: Cyan
 * - Position 8: Light Gray
 * - Position 9: Dark Gray
 * - Position 10: Light Red
 * - Position 11: Light Green
 * - Position 12: Light Yellow
 * - Position 13: Light Blue
 * - Position 14: Light Magenta
 * - Position 15: Light Cyan
 * - Position 16: White
 *
 * @note The color codes are compatible with most text-based terminal environments.
 */
inline std::array<OSTDString, 17> GAllLogColor{
    "\033[0m",  //Reset
    "\033[30m", //Black
    "\033[31m", //Red
    "\033[32m", //Green
    "\033[33m", //Yellow
    "\033[34m", //Blue
    "\033[35m", //Magenta
    "\033[36m", //Cyan
    "\033[37m", //Light Gray
    "\033[90m", //Dark Gray
    "\033[91m", //Light Red
    "\033[92m", //LightGreen
    "\033[93m", //Light Yellow
    "\033[94m", //Light Blue
    "\033[95m", //Light Magenta
    "\033[96m", //Light Cyan
    "\033[97m", //White    
};