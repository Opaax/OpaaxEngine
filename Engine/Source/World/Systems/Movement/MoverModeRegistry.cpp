#include "World/Systems/Movement/MoverModeRegistry.h"

namespace Opaax
{
    IMoverMode* MoverModeRegistry::Find(const OpaaxStringID InName) const noexcept
    {
        if (!InName.IsValid())
        {
            return nullptr;
        }

        // Linear over a handful of integer compares — a project has modes, not thousands of them.
        for (const Entry& lEntry : m_Entries)
        {
            if (lEntry.Name == InName)
            {
                return lEntry.Mode.get();
            }
        }

        return nullptr;
    }
}
