#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/World/Guid.h"
#include "Core/World/EntityTypes.h"

namespace Opaax
{
    // =============================================================================
    // GuidRegistry — per-World lookup from a stable Guid to the runtime EntityID that
    //   currently carries it. Owned by a single World: entt handles are only valid
    //   inside their own registry, so a Guid is resolved through its world and never
    //   crosses worlds. Missing lookups resolve to ENTITY_NONE (null-safe).
    // =============================================================================
    class OPAAX_API GuidRegistry
    {
        // =========================================================================
        // Registration
        // =========================================================================
    public:
        // Map InGuid -> InEntity (replaces any existing mapping for InGuid).
        void Register(const Guid& InGuid, EntityID InEntity);

        // Drop the mapping for InGuid, if present.
        void Unregister(const Guid& InGuid);

        // Runtime entity for InGuid, or ENTITY_NONE when unknown.
        EntityID Resolve(const Guid& InGuid) const noexcept;

        // Drop every mapping.
        void Clear() noexcept;

        // =========================================================================
        // Query
        // =========================================================================
    public:
        bool   Contains(const Guid& InGuid) const noexcept { return m_Map.find(InGuid) != m_Map.end(); }
        Uint64 Count() const noexcept                      { return static_cast<Uint64>(m_Map.size()); }

        // =========================================================================
        // Members
        // =========================================================================
    private:
        UnorderedMap<Guid, EntityID> m_Map;
    };
}
