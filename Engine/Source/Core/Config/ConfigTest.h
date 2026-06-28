#pragma once

#include "Core/Config/TConfig.hpp"

using namespace Opaax;

struct MyConfigData
{
    
    static MyConfigData Parse(const OpaaxString& InJsonText) { return MyConfigData(); }
    static OpaaxString Serialize(const MyConfigData& InData) { return OpaaxString(); }
};

DECLARE_OPAAX_T_CONFIG_CODEC(MyConfigData::Parse, MyConfigData::Serialize, MyConfigData)

DECLARE_OPAAX_T_CONFIG(MyConfig, MyConfigData)
