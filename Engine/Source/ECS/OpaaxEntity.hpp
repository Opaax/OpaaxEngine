#pragma once

#include "Core/OpaaxTypes.h"
#include "Core/EngineAPI.h"

namespace Opaax::ECS
{
    inline constexpr Uint32 ENTITY_INDEX_BITS = 20u;
    inline constexpr Uint32 ENTITY_GEN_BITS   = 12u;

    inline constexpr Uint32 ENTITY_INDEX_MASK = (1u << ENTITY_INDEX_BITS)   - 1u;  // 0x000FFFFF
    inline constexpr Uint32 ENTITY_GEN_MASK   = (1u << ENTITY_GEN_BITS)     - 1u;  // 0x00000FFF

    inline constexpr Uint32 ENTITY_ID_NONE    = 0u;

    /**
     * @struct EntityID
     *
     * Packed 32-bit handle:
     *   bits  0–19 : index    (up to 1 048 576 live entities)
     *   bits 20–31 : generation (up to 4096 reuses per slot)
     *
     * NOTE: Generation prevents stale-handle access.
     *  If you store an EntityID and the entity is destroyed + slot reused, the generation mismatch will be caught by World::IsValid().
     */
    struct OPAAX_API EntityID
    {

        // =============================================================================
        // Static
        // =============================================================================

        static constexpr EntityID Make(Uint32 InIndex, Uint32 InGeneration) noexcept
        {
            return EntityID((InGeneration << ENTITY_INDEX_BITS) | (InIndex & ENTITY_INDEX_MASK));
        }
        
        // =============================================================================
        // CTORs
        // =============================================================================
        
        constexpr EntityID() noexcept : m_Value(ENTITY_ID_NONE) {}
        constexpr explicit EntityID(Uint32 InValue) noexcept : m_Value(InValue) {}

        // =============================================================================
        // Functions
        // =============================================================================

        //------------------------------------------------------------------------------
        //  Get - Set
        
        constexpr Uint32 GetIndex()      const noexcept { return m_Value & ENTITY_INDEX_MASK; }
        constexpr Uint32 GetGeneration() const noexcept { return (m_Value >> ENTITY_INDEX_BITS) & ENTITY_GEN_MASK; }
        constexpr Uint32 GetValue()      const noexcept { return m_Value; }
        constexpr bool   IsValid()       const noexcept { return m_Value != ENTITY_ID_NONE; }

        // =============================================================================
        // Operators
        // =============================================================================
        constexpr bool operator==(const EntityID& Other) const noexcept { return m_Value == Other.m_Value; }
        constexpr bool operator!=(const EntityID& Other) const noexcept { return m_Value != Other.m_Value; }

        // =============================================================================
        // Members
        // =============================================================================
    private:
        Uint32 m_Value;
    };

    inline constexpr EntityID ENTITY_NONE = EntityID{};

} // namespace Opaax::ECS

// =============================================================================
// Hash support — EntityID be used as unordered_map key
// =============================================================================
namespace std
{
    template<>
    struct hash<Opaax::ECS::EntityID>
    {
        size_t operator()(const Opaax::ECS::EntityID& InID) const noexcept
        {
            return std::hash<Opaax::Uint32>{}(InID.GetValue());
        }
    };
}