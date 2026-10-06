#include "World/Systems/Movement/MoverModeRegistry.h"

namespace Opaax
{
    IMoverMode* MoverModeRegistry::Find(const OpaaxStringID InName) const noexcept
    {
        if (!InName.IsValid())
        {
            return nullptr;
        }

        // Linear search: there are few modes.
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
