#pragma once

#pragma once

#include "OpaaxEntity.hpp"
#include "Core/OpaaxTypes.h"
#include "Core/Log/OpaaxLog.h"

namespace Opaax::ECS
{
    /**
     * @class ComponentStorage
     *
     * Dense packed array of components.
     *  Lookup : EntityID → dense index via sparse array (direct index into m_Sparse).
     *  Iteration : tight loop over m_Dense / m_Components — cache friendly.
     *
     *  Capacity: ENTITY_INDEX_MASK + 1 sparse slots (1M).
     *      In practice only live entities consume dense slots.
     *
     *  Remove uses swap-and-pop to keep the dense array packed.
     *      This means component order is NOT stable across removals.
     *      Do not store raw indices into the dense array externally.
     * @tparam T 
     */
    template<typename T>
    class ComponentStorage
    {
        // =============================================================================
        // Static
        // =============================================================================
        
        // Slot is empty
        static constexpr Uint32 INVALID_DENSE_INDEX = ~0u;

        // =============================================================================
        // CTORs
        // =============================================================================
    public:
        ComponentStorage()
        {
            // NOTE: Sparse array is pre-allocated to max entity count.
            //   1M * 4 bytes = 4 MB resident per storage. Acceptable for a desktop 2D engine.
            //   If this becomes a problem, switch to a hash map for sparse lookup.
            // FIXME: Make capacity configurable via a template param or engine config
            m_Sparse.resize(ENTITY_INDEX_MASK + 1, INVALID_DENSE_INDEX);
        }

        // =============================================================================
        // Function
        // =============================================================================
    public:
        /**
         * Add — constructs component in-place
         * Asserts if the entity already has this component type.
         * @tparam Args 
         * @param InEntity 
         * @param InArgs 
         * @return ref to the new component.
         */
        template<typename... Args>
        T& Add(EntityID InEntity, Args&&... InArgs)
        {
            const Uint32 lIdx = InEntity.GetIndex();

            OPAAX_CORE_ASSERT(lIdx <= ENTITY_INDEX_MASK)
            OPAAX_CORE_ASSERT(m_Sparse[lIdx] == INVALID_DENSE_INDEX) // already has component

            const Uint32 lDenseIdx = static_cast<Uint32>(m_Components.size());

            m_Sparse[lIdx] = lDenseIdx;
            m_Dense.push_back(InEntity);
            m_Components.emplace_back(std::forward<Args>(InArgs)...);

            return m_Components.back();
        }
        
        /**
         * Remove — swap-and-pop, O(1)
         * @param InEntity  
         */
        void Remove(EntityID InEntity)
        {
            const Uint32 lIdx = InEntity.GetIndex();

            if (lIdx > ENTITY_INDEX_MASK || m_Sparse[lIdx] == INVALID_DENSE_INDEX)
            {
                OPAAX_CORE_WARN("ComponentStorage::Remove — entity has no component of this type.");
                return;
            }

            const Uint32 lDenseIdx    = m_Sparse[lIdx];
            const Uint32 lLastDense   = static_cast<Uint32>(m_Components.size()) - 1;

            if (lDenseIdx != lLastDense)
            {
                // Swap with last, fix up sparse entry for the moved entity
                m_Components[lDenseIdx]                 = std::move(m_Components[lLastDense]);
                m_Dense[lDenseIdx]                      = m_Dense[lLastDense];
                m_Sparse[m_Dense[lDenseIdx].GetIndex()] = lDenseIdx;
            }

            m_Components.pop_back();
            m_Dense.pop_back();
            m_Sparse[lIdx] = INVALID_DENSE_INDEX;
        }

        //------------------------------------------------------------------------------
        //  Get - Set

        /**
         * 
         * @param InEntity 
         * @return 
         */
        T* Get(EntityID InEntity) noexcept
        {
            const Uint32 lIdx = InEntity.GetIndex();
            if (lIdx > ENTITY_INDEX_MASK || m_Sparse[lIdx] == INVALID_DENSE_INDEX)
            {
                return nullptr;
            }
            return &m_Components[m_Sparse[lIdx]];
        }

        /**
         * 
         * @param InEntity 
         * @return 
         */
        const T* Get(EntityID InEntity) const noexcept
        {
            const Uint32 lIdx = InEntity.GetIndex();
            if (lIdx > ENTITY_INDEX_MASK || m_Sparse[lIdx] == INVALID_DENSE_INDEX)
            {
                return nullptr;
            }
            return &m_Components[m_Sparse[lIdx]];
        }

        /**
         * 
         * @param InEntity 
         * @return 
         */
        bool Has(EntityID InEntity) const noexcept
        {
            const Uint32 lIdx = InEntity.GetIndex();
            return lIdx <= ENTITY_INDEX_MASK && m_Sparse[lIdx] != INVALID_DENSE_INDEX;
        }

        // =============================================================================
        // Iteration — direct access to dense arrays
        // PERF: Iterate over GetComponents() directly in systems — tight cache loop.
        // =============================================================================

        /***/
        FORCEINLINE TDynArray<T>&               GetComponents() noexcept       { return m_Components; }
        /***/
        FORCEINLINE const TDynArray<T>&         GetComponents() const noexcept { return m_Components; }
        /***/
        FORCEINLINE const TDynArray<EntityID>&  GetEntities()   const noexcept { return m_Dense; }

        FORCEINLINE Uint32 Count() const noexcept { return static_cast<Uint32>(m_Components.size()); }

        // =============================================================================
        // Members
        // =============================================================================
    private:
        TDynArray<Uint32>    m_Sparse;      // entity index → dense index
        TDynArray<EntityID>  m_Dense;       // dense index → entity (for reverse lookup)
        TDynArray<T>         m_Components;  // dense, packed, cache-friendly
    };

} // namespace Opaax::ECS