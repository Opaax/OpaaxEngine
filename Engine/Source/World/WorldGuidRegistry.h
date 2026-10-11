#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/GUID/Guid.h"
#include "World/Entity/EntityTypes.h"

namespace Opaax
{
    // =============================================================================
    // GuidRegistry — per-World lookup from Guid to EntityID. Unknown guids give ENTITY_NONE.
    // =============================================================================
    class WorldGuidRegistry
    {
        // =========================================================================
        // Functions
        // =========================================================================
        
        // =========================================================================
        // Registration
    public:
        /**
         * Maps InGuid to InEntity (replaces any existing mapping).
         */
        void Register(const Guid& InGuid, EntityID InEntity);
        
        /**
         * Removes the mapping for InGuid, if any.
         */
        void Unregister(const Guid& InGuid);

        /**
         * @return The entity for InGuid, or ENTITY_NONE
         */
        EntityID Resolve(const Guid& InGuid) const noexcept;

        /**
         * Removes every mapping.
         */
        void Clear() noexcept;
        
        // End Registration
        // =========================================================================

        // =========================================================================
        // Query
    public:
        /**
         * @return True if InGuid is mapped
         */
        bool   Contains(const Guid& InGuid) const noexcept { return m_Map.find(InGuid) != m_Map.end(); }

        /**
         * @return Number of mapped guids
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
