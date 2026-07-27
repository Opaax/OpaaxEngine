#pragma once

#include "Core/String/OpaaxString.hpp"
#include "Editor/Panels/IEditorPanel.h"

namespace Opaax::Editor
{
    struct EditorContext;

    // =============================================================================
    // InspectorPanel — the dockable "Inspector": the editor's READER of EditorSelection (Hierarchy is
    //   the writer), showing the selected entity's components through the registered drawers.
    //
    //   It knows nothing about any component type. It walks EditorContext::Extensions.Drawers() and
    //   invokes each entry, and each entry self-checks whether it applies to this entity — so a game
    //   module's component becomes inspectable purely by registering a drawer, with no editor change.
    //   That inversion is what makes component reflection unnecessary (see DrawerRegistry).
    //
    //   No change notification: ImGui is immediate-mode, so Draw() simply renders whatever the selection
    //   is right now. An event would be stored and then read here anyway (user decision, 2026-07-27).
    // =============================================================================
    class InspectorPanel final : public IEditorPanel
    {
        // =============================================================================
        // Ctor - Dtor
        // =============================================================================
    public:
        explicit InspectorPanel(EditorContext& InContext);
        ~InspectorPanel() override;

        // =============================================================================
        // Copy delete
        // =============================================================================
    public:
        InspectorPanel(const InspectorPanel&)            = delete;
        InspectorPanel& operator=(const InspectorPanel&) = delete;

        // =============================================================================
        // Override
        // =============================================================================
    public:
        //~Begin IEditorPanel interface
        /** No resource to acquire — the panel reads the selection and the routes through the context. */
        void            Startup()               override {}

        /** Nothing the world's render depends on. */
        void            OnPreRender()           override {}

        /**
         * Empty states are explicit text, never a blank panel (L12), and they are distinct on purpose:
         * "Nothing selected." (no entity) vs "No drawable components." (an entity, but no registered
         * drawer applied to it) — the second is why FDrawerInvoke reports whether it drew.
         */
        void            Draw()                  override;

        /** No resource to release. */
        void            Shutdown()              override {}

        OpaaxStringID   GetPanelID()    const   override { return m_PanelID; }
        //~End IEditorPanel interface

        // =============================================================================
        // Members
        // =============================================================================
    private:
        EditorContext& m_Context;

        const OpaaxStringID m_PanelID{ OPAAX_ID("Inspector") };
        const OpaaxString   m_Title = m_PanelID.ToString();
    };
}
