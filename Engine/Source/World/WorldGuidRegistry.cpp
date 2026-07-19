#include "WorldGuidRegistry.h"

namespace Opaax
{
    void WorldGuidRegistry::Register(const Guid& InGuid, EntityID InEntity)
    {
        m_Map[InGuid] = InEntity;
    }

    void WorldGuidRegistry::Unregister(const Guid& InGuid)
    {
        m_Map.erase(InGuid);
    }

    EntityID WorldGuidRegistry::Resolve(const Guid& InGuid) const noexcept
    {
        const auto lIt = m_Map.find(InGuid);
        return (lIt != m_Map.end()) ? lIt->second : ENTITY_NONE;
    }

    void WorldGuidRegistry::Clear() noexcept
    {
        m_Map.clear();
    }
}
