#include "ConfigTest.h"


MyConfigData MyConfigData::Parse(const Opaax::OpaaxString& InJsonText)
{
    return MyConfigData();
}

Opaax::OpaaxString MyConfigData::Serialize(const MyConfigData& InData)
{
    return Opaax::OpaaxString();
}

IMPL_OPAAX_T_CONFIG(MyConfig)