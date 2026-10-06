#include "Resources/ResourceFormat.h"

#include "Core/String/OpaaxString.hpp"

namespace Opaax
{
    OpaaxStringID NormalizeExtension(OpaaxStringView InExtension)
    {
        if (InExtension.IsEmpty())
        {
            return {};
        }

        const OpaaxString lLower = InExtension.ToString().ToLower();

        // Accept "wave" or ".wave"; always store the dotted form.
        return OpaaxStringID(lLower[0] == '.' ? lLower : (OpaaxString(".") + lLower));
    }
}
