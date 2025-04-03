#pragma once
#include <array>
#include "../OpaaxTypes.hpp"

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

inline std::array<OString, 17> GAllLogColor{
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