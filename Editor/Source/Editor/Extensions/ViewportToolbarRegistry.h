#pragma once

#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxStringID.hpp"

#include <imgui.h>

namespace Opaax::Editor
{
    struct EditorContext;

    // =============================================================================
    // ViewportToolbarRegistry — WHAT sits on the strip over the viewport, in registration order.
    //
    //   The eighth route on EditorExtensionRegistrar, registered natives-first then modules then
    //   sealed (MR2), so a game module adds a tool with no change to OpaaxEditorLib — the same route
    //   a panel, a menu entry or a drawer takes.
    //
    //   AN ITEM IS A CLOSURE, WHERE A MENU NODE IS A TAG, and the difference is principled rather
    //   than a relapse (L37/L38 retired MenuRegistry's closures for exactly the opposite reason). A
    //   menu node has ONE behaviour — invoke a command — so a tag says everything about it. A toolbar
    //   item is a WIDGET: a toggle, a drag-float, a combo, a colour swatch. There is no uniform
    //   behaviour to name, so tags here would mean a new item TYPE per widget kind, which is the
    //   machinery "easy to add things to it" exists to avoid.
    //
    //   An item that wants to INVOKE something still should: the native mode buttons dispatch
    //   EDITOR_COMMAND_GIZMO_* by tag rather than calling EditorGizmo, so the toolbar, the Edit menu
    //   and W/E/R remain three front-ends onto one command.
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
         * Append one tool. InId is the item's ImGui ID SCOPE as well as its name, so two items that
         * happen to label a widget the same cannot fight over hover and active state — I15's
         * entry-is-an-ID-scope rule, which the Inspector needed for exactly this reason.
         */
        void Add(const OpaaxStringID InId, DrawFunc InDraw)
        {
            if (!InId.IsValid() || InDraw == nullptr) { return; }

            m_Items.emplace_back(InId, std::move(InDraw), false);
        }

        /** A vertical rule between groups. Collapses if it would lead or double up. */
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
         * Draw every tool on one line, in registration order.
         *
         * SameLine between items rather than inside them: an item author writes ordinary ImGui and
         * does not have to know it is on a strip, which is most of what makes adding one cheap.
         */
        void Draw(EditorContext& InContext) const
        {
            bool bFirst = true;

            for (const Item& lItem : m_Items)
            {
                if (!bFirst) { ImGui::SameLine(); }
                bFirst = false;

                if (lItem.bSeparator)
                {
                    ImGui::TextDisabled("|");
                    continue;
                }

                ImGui::PushID(lItem.Id.CStr());
                lItem.Draw(InContext);
                ImGui::PopID();
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
