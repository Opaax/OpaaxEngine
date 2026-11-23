#include "UI/UILayerTable.h"

#include "Core/Algo/Algo.h"

namespace Opaax
{
    namespace
    {
        /** How many dotted names InTag has ("Menu.Pause": 2). */
        Uint32 CountNames(const OpaaxTag InTag) noexcept
        {
            const OpaaxStringView lText  = InTag.GetView();
            Uint32                lCount = lText.IsEmpty() ? 0u : 1u;
            for (Uint32 i = 0; i < lText.GetLength(); ++i)
            {
                if (lText[i] == TAG_SEPARATOR) { ++lCount; }
            }
            return lCount;
        }
    }

    // =============================================================================
    // UILayerKey
    // =============================================================================

    OpaaxString UILayerKey::ToString() const
    {
        if (Depth == 0)
        {
            return OpaaxString("0");
        }

        // "/" between orders: a dot would read "100.-10" for a negative sub-layer.
        OpaaxString lText;
        for (Uint32 i = 0; i < Depth && i < MAX_DEPTH; ++i)
        {
            if (i > 0) { lText += '/'; }
            lText += OpaaxString::Format("{}", Orders[i]);
        }
        return lText;
    }

    // =============================================================================
    // UILayerTable
    // =============================================================================

    UILayerTable UILayerTable::Build(const TDynArray<UILayerSettings>& InSettings, TDynArray<OpaaxString>& OutProblems)
    {
        UILayerTable lTable;

        // Every name first: a sub-layer's key reads its parents' orders, wherever they are in the list.
        for (const UILayerSettings& lLayer : InSettings)
        {
            if (!lLayer.Name.IsValid())
            {
                OutProblems.push_back(OpaaxString("A layer has no name (UI.Layers): it is left out"));
                continue;
            }
            if (lTable.Find(lLayer.Name) != nullptr)
            {
                OutProblems.push_back(OpaaxString::Format("Layer '{}' is listed twice (UI.Layers): the first is kept", lLayer.Name));
                continue;
            }
            if (CountNames(lLayer.Name) > UILayerKey::MAX_DEPTH)
            {
                OutProblems.push_back(OpaaxString::Format("Layer '{}' is more than {} names deep (UI.Layers): it is left out",
                                                          lLayer.Name, UILayerKey::MAX_DEPTH));
                continue;
            }
            lTable.m_Layers.push_back(Entry{ lLayer.Name, lLayer.Order, {} });
        }

        // Each key: its parents' orders from the top, then its own. A parent not listed counts as 0.
        for (Entry& lEntry : lTable.m_Layers)
        {
            TFixedArray<OpaaxTag, UILayerKey::MAX_DEPTH> lChain{};   // the layer first, the top-level name last
            Uint32 lDepth = 0;
            for (OpaaxTag lNode = lEntry.Name; lNode.IsValid() && lDepth < UILayerKey::MAX_DEPTH; lNode = lNode.GetParent())
            {
                lChain[lDepth++] = lNode;
            }

            lEntry.Key.Depth = lDepth;
            for (Uint32 i = 0; i < lDepth; ++i)
            {
                const OpaaxTag     lNode   = lChain[lDepth - 1 - i];
                const Entry* const lListed = lTable.Find(lNode);
                if (lListed == nullptr)
                {
                    OutProblems.push_back(OpaaxString::Format("Layer '{}' is under '{}', which is not listed (UI.Layers): "
                                                              "'{}' counts as 0", lEntry.Name, lNode, lNode));
                }
                lEntry.Key.Orders[i] = (lListed != nullptr) ? lListed->Order : 0;
            }
        }

        Algo::StableSort(lTable.m_Layers, [](const Entry& InA, const Entry& InB) { return InA.Key < InB.Key; });
        return lTable;
    }

    OpaaxTag UILayerTable::Resolve(const OpaaxTag InLayer) const
    {
        for (OpaaxTag lNode = InLayer; lNode.IsValid(); lNode = lNode.GetParent())
        {
            if (Find(lNode) != nullptr)
            {
                return lNode;
            }
        }
        return OpaaxTag();
    }

    UILayerKey UILayerTable::KeyOf(const OpaaxTag InLayer) const
    {
        const Entry* const lListed = Find(Resolve(InLayer));
        return (lListed != nullptr) ? lListed->Key : UILayerKey{};
    }

    const UILayerTable::Entry* UILayerTable::Find(const OpaaxTag InLayer) const noexcept
    {
        if (!InLayer.IsValid())
        {
            return nullptr;
        }

        // A handful of layers: a scan is all it takes.
        for (const Entry& lEntry : m_Layers)
        {
            if (lEntry.Name == InLayer)
            {
                return &lEntry;
            }
        }
        return nullptr;
    }
}
