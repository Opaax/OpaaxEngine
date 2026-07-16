#include "GuidRegistry.h"

namespace Opaax
{
    void GuidRegistry::Register(const Guid& InGuid, EntityID InEntity)
    {
        m_Map[InGuid] = InEntity;
    }

    void GuidRegistry::Unregister(const Guid& InGuid)
    {
        m_Map.erase(InGuid);
    }

    EntityID GuidRegistry::Resolve(const Guid& InGuid) const noexcept
    {
        const auto lIt = m_Map.find(InGuid);
        return (lIt != m_Map.end()) ? lIt->second : ENTITY_NONE;
    }

    void GuidRegistry::Clear() noexcept
    {
        m_Map.clear();
    }
}
