#pragma once

#include "Core/OpaaxTypes.h"

// =============================================================================
// ResourceDependencyGraph — parent -> child and child -> parent links between resources,
// keyed by path id. Recorded by LoadContext::Acquire, removed on unload.
// Meant for hot reload (which resources depend on this file?).
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
         * Records that Parent depends on Child (Level -> Texture).
         */
        void AddEdge(Uint32 InParentId, Uint32 InChildId)
        {
            AddUnique(m_Forward[InParentId], InChildId);
            AddUnique(m_Reverse[InChildId], InParentId);
        }

        /**
         * Removes every link of InNodeId (both directions). Called on unload.
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
            
            InList.emplace_back(InValue);
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
