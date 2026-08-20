#include "ResourceFormat.h"

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

        // A registrant may write "wave" or ".wave"; a scanner always produces the dotted form, so the
        // dot is added here rather than trusted from either side.
        return OpaaxStringID(lLower[0] == '.' ? lLower : (OpaaxString(".") + lLower));
    }
}
