#include "Engine/Config/Config.h"

#include "Core/String/OpaaxPathString.h"

namespace Opaax
{
    OpaaxStringID Config::NameFromFile(const char* InFileName)
    {
        return OpaaxStringID(PathString::Stem(InFileName).ToString());
    }
}
