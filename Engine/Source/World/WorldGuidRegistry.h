#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/GUID/Guid.h"
#include "World/Entity/EntityTypes.h"

namespace Opaax
{
    // =============================================================================
    // GuidRegistry — per-World lookup from a stable Guid to the runtime EntityID that
    //   currently carries it. Owned by a single World: entt handles are only valid
    //   inside their own registry, so a Guid is resolved through its world and never
    //   crosses worlds. Missing lookups resolve to ENTITY_NONE (null-safe).
    // =============================================================================
    class OPAAX_API WorldGuidRegistry
    {
        // =========================================================================
        // Functions
        // =========================================================================
        
        // =========================================================================
        // Registration
    public:
        /**
         * Map InGuid -> InEntity (replaces any existing mapping for InGuid).
         * @param InGuid 
         * @param InEntity 
         */
        void Register(const Guid& InGuid, EntityID InEntity);
        
        /**
         * Drop the mapping for InGuid, if present.
         * @param InGuid 
         */
        void Unregister(const Guid& InGuid);

        /**
         * Runtime entity for InGuid, or ENTITY_NONE when unknown.
         * @param InGuid 
         * @return 
         */
        EntityID Resolve(const Guid& InGuid) const noexcept;

        /**
         * Drop every mapping.
         */
        void Clear() noexcept;
        
        // End Registration
        // =========================================================================

        // =========================================================================
        // Query
    public:
        /**
         * @param InGuid the GUID to check
         * @return true if the GUID is mapped
         */
        bool   Contains(const Guid& InGuid) const noexcept { return m_Map.find(InGuid) != m_Map.end(); }

        /**
         * @return The count of mapped GUID
         */
        Uint64 Count() const noexcept                      { return static_cast<Uint64>(m_Map.size()); }
        // End Query
        // =========================================================================

        // =========================================================================
        // Members
        // =========================================================================
    private:
        TUnorderedMap<Guid, EntityID> m_Map;
    };
}
