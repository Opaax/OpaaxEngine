#pragma once

#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"
#include "Core/Tag/OpaaxTag.h"
#include "UI/UILayerSettings.h"

namespace Opaax
{
    /**
     * Where a layer draws: its orders from the top level down ("Menu.Pause" at 10 under Menu at 100 is
     * {100, 10}). Compared one order at a time, a missing one counting as 0, so a sub-layer never leaves
     * its parent's place.
     */
    struct UILayerKey
    {
        static constexpr Uint32 MAX_DEPTH = 4;

        TFixedArray<Int32, MAX_DEPTH> Orders{};   // past Depth: 0
        Uint32                        Depth = 0;

        /** Negative, zero or positive as InA draws under, level with or over InB. */
        static Int32 Compare(const UILayerKey& InA, const UILayerKey& InB) noexcept
        {
            for (Uint32 i = 0; i < MAX_DEPTH; ++i)
            {
                if (InA.Orders[i] != InB.Orders[i])
                {
                    return InA.Orders[i] < InB.Orders[i] ? -1 : 1;
                }
            }
            return 0;
        }

        friend bool operator==(const UILayerKey& InA, const UILayerKey& InB) noexcept { return Compare(InA, InB) == 0; }
        friend bool operator!=(const UILayerKey& InA, const UILayerKey& InB) noexcept { return Compare(InA, InB) != 0; }
        friend bool operator<(const UILayerKey& InA, const UILayerKey& InB) noexcept  { return Compare(InA, InB) < 0; }
        friend bool operator>(const UILayerKey& InA, const UILayerKey& InB) noexcept  { return Compare(InA, InB) > 0; }

        /** "100.10": the orders down to Depth ("0" for none). */
        OpaaxString ToString() const;
    };

    // =============================================================================
    // UILayerTable — the game canvas's layers, read from a list of named orders (UI.Layers). Pure: it
    //   says where a layer draws; the canvas keeps its trees in that order.
    // =============================================================================
    class UILayerTable
    {
    public:
        struct Entry
        {
            OpaaxTag   Name;
            Int32      Order = 0;
            UILayerKey Key;
        };

        /**
         * The layers InSettings lists. What is wrong with the list goes to OutProblems, one line each, and is
         * left out (no name, listed twice, deeper than MAX_DEPTH) or worked around (an unlisted parent counts as 0).
         */
        static UILayerTable Build(const TDynArray<UILayerSettings>& InSettings, TDynArray<OpaaxString>& OutProblems);

        /** InLayer if listed, else its nearest listed parent, else the invalid tag. */
        OpaaxTag Resolve(OpaaxTag InLayer) const;

        /** Where InLayer draws: its listed key, its nearest listed parent's, else 0 (a tree with no layer). */
        UILayerKey KeyOf(OpaaxTag InLayer) const;

        bool IsListed(OpaaxTag InLayer) const { return Find(InLayer) != nullptr; }

        /** The listed layers, bottom to top (list order between equal keys). */
        const TDynArray<Entry>& GetLayers() const noexcept { return m_Layers; }

    private:
        const Entry* Find(OpaaxTag InLayer) const noexcept;

        TDynArray<Entry> m_Layers;
    };
}
