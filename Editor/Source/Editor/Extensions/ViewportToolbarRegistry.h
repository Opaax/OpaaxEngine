#pragma once

#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxStringID.hpp"

#include "Editor/EditorContext.h"
#include "Editor/UI/IEditorWidgets.h"

namespace Opaax::Editor
{
    struct EditorContext;

    // =============================================================================
    // ViewportToolbarRegistry — what sits on the strip over the viewport, in registration order.
    //   Natives register first, then modules, then it seals, so a game module can add a tool.
    //   An item is a closure (a widget: toggle, drag-float, combo...), unlike a menu node which is a
    //   command tag. Items that trigger actions should still dispatch commands by tag.
    // =============================================================================
    class ViewportToolbarRegistry
    {
        // =============================================================================
        // Types
        // =============================================================================
    public:
        using DrawFunc = TFunction<void(EditorContext&)>;

        struct Item
        {
            OpaaxStringID Id;          // invalid for a separator
            DrawFunc      Draw;        // null for a separator
            bool          bSeparator = false;
        };

        // =============================================================================
        // Register
        // =============================================================================
    public:
        /**
         * Appends one tool. InId is also the item's ImGui ID scope, so two items with the same widget
         * label do not collide.
         */
        void Add(const OpaaxStringID InId, DrawFunc InDraw)
        {
            if (!InId.IsValid() || InDraw == nullptr) { return; }

            m_Items.emplace_back(InId, std::move(InDraw), false);
        }

        /** A vertical separator between groups. Skipped if it would lead or repeat. */
        void AddSeparator()
        {
            if (m_Items.empty() || m_Items.back().bSeparator) { return; }

            m_Items.emplace_back(OpaaxStringID{}, DrawFunc{}, true);
        }

        // =============================================================================
        // Consume
        // =============================================================================
    public:
        /**
         * Draws every tool on one line, in registration order. SameLine is added between items, so an item
         * is written as ordinary ImGui.
         */
        void Draw(EditorContext& InContext) const
        {
            IEditorWidgets& lWidgets = InContext.Widgets;

            bool bFirst = true;

            for (const Item& lItem : m_Items)
            {
                if (!bFirst) { lWidgets.SameLine(); }
                bFirst = false;

                if (lItem.bSeparator)
                {
                    lWidgets.ToolbarSeparator();
                    continue;
                }

                lWidgets.PushId(lItem.Id.CStr());
                lItem.Draw(InContext);
                lWidgets.PopId();
            }
        }

        Uint64 Count() const noexcept { return m_Items.size(); }
        bool   IsEmpty() const noexcept { return m_Items.empty(); }

        // =============================================================================
        // Members
        // =============================================================================
    private:
        TDynArray<Item> m_Items;
    };
}
