#pragma once

#include "Core/Config/TConfig.hpp"

using namespace Opaax;

struct MyConfigData
{
    DECLARE_CONFIG_DATA(MyConfigData)
};

DECLARE_T_CONFIG_CODEC(MyConfigData::Parse, MyConfigData::Serialize, MyConfigData)

DECLARE_T_CONFIG(MyConfig, MyConfigData)
