#pragma once

#include "Core/OpaaxTypes.h"

// =============================================================================
// ResourceDependencyGraph — forward + reverse edges between resources, keyed by
// interned path id (OpaaxStringID). LoadContext::Acquire records an edge on every
// composite acquisition; unload drops the node's edges.
//
//   M-RES-1: RECORDED, not consumed. M-RES-3 hot reload walks the REVERSE edges to
//   answer "TextureA changed on disk — who is dirty?". Kept tiny and consumer-free
//   on purpose (anti-god-object border, review C5/C6).
// =============================================================================
namespace Opaax
{
    class ResourceDependencyGraph final
    {
        // =============================================================================
        // Functions
        // =============================================================================
    public:
        /**
         * Parent hard-depends on Child (Level -> Texture). 
         * Down-the-DAG only.
         * @param InParentId 
         * @param InChildId 
         */
        void AddEdge(Uint32 InParentId, Uint32 InChildId)
        {
            AddUnique(m_Forward[InParentId], InChildId);
            AddUnique(m_Reverse[InChildId], InParentId);
        }

        /**
         * Drop every edge touching InNodeId (both directions) — called on unload.
         * @param InNodeId 
         */
        void RemoveNode(Uint32 InNodeId)
        {
            if (const auto lIt = m_Forward.find(InNodeId); lIt != m_Forward.end())
            {
                for (const Uint32 lChild : lIt->second) { RemoveFrom(m_Reverse, lChild, InNodeId); }
                m_Forward.erase(lIt);
            }
            if (const auto lIt = m_Reverse.find(InNodeId); lIt != m_Reverse.end())
            {
                for (const Uint32 lParent : lIt->second) { RemoveFrom(m_Forward, lParent, InNodeId); }
                m_Reverse.erase(lIt);
            }
        }

        /***/
        void Clear() noexcept { m_Forward.clear(); m_Reverse.clear(); }

        
        // =============================================================================
        // Getter
        /**
         * Dependencies of InNodeId (what it hard-references).
         * @param InNodeId 
         * @return 
         */
        const TDynArray<Uint32>* GetDependencies(Uint32 InNodeId) const { return Find(m_Forward, InNodeId); }

        /**
         * Dependents of InNodeId (who hard-references it) — the M3 dirty set.
         * @param InNodeId 
         * @return 
         */
        const TDynArray<Uint32>* GetDependents(Uint32 InNodeId)   const { return Find(m_Reverse, InNodeId); }
        
        // End Getter
        // =============================================================================

        // =============================================================================
        // Internal
        // =============================================================================
    private:
        static void AddUnique(TDynArray<Uint32>& InList, Uint32 InValue)
        {
            for (const Uint32 lExisting : InList)
            {
                if (lExisting == InValue)
                {
                    return;
                }
            }
            
            InList.push_back(InValue);
        }

        static void RemoveFrom(TUnorderedMap<Uint32, TDynArray<Uint32>>& InMap, Uint32 InKey, Uint32 InValue)
        {
            const auto lIt = InMap.find(InKey);
            if (lIt == InMap.end())
            {
                return;
            }
            
            TDynArray<Uint32>& lList = lIt->second;
            for (Uint32 i = 0; i < lList.size(); ++i)
            {
                if (lList[i] == InValue) { lList[i] = lList.back(); lList.pop_back(); break; }
            }
            
            if (lList.empty())
            {
                InMap.erase(lIt);
            }
        }

        static const TDynArray<Uint32>* Find(const TUnorderedMap<Uint32, TDynArray<Uint32>>& InMap, Uint32 InKey)
        {
            const auto lIt = InMap.find(InKey);
            return (lIt != InMap.end()) ? &lIt->second : nullptr;
        }

        // =============================================================================
        // Members
        // =============================================================================
    private:
        TUnorderedMap<Uint32, TDynArray<Uint32>> m_Forward; // parent -> children
        TUnorderedMap<Uint32, TDynArray<Uint32>> m_Reverse; // child  -> parents
    };
}
