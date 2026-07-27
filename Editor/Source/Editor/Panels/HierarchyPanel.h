#pragma once

#include "Application/Services/ILogger.h"
#include "Core/String/OpaaxString.hpp"
#include "Editor/Panels/IEditorPanel.h"

namespace Opaax
{
    OPAAX_LOG_CATEGORY(HierarchyPanel)
}

namespace Opaax::Editor
{
    struct EditorContext;

    // =============================================================================
    // HierarchyPanel — the dockable "Hierarchy" panel: one selectable row per entity in the active
    //   world, and the editor's single WRITER of EditorSelection (the Inspector, M2b, is the reader).
    //
    //   Enumerates through World::Each<EntityMeta>, which is the all-entities view by construction —
    //   World::CreateEntity always emplaces EntityMeta — so listing entities needs no World/ECS API.
    //
    //   It is a NATIVE editor panel with no privileges: EditorService registers it into the same
    //   PanelRegistry a game module registers into, and it is constructed by the same loop. That
    //   symmetry is the thing M2a exists to prove, so keep it registered — never hand-built.
    // =============================================================================
    class HierarchyPanel final : public IEditorPanel
    {
        // =============================================================================
        // Ctor - Dtor
        // =============================================================================
    public:
        explicit HierarchyPanel(EditorContext& InContext);
        ~HierarchyPanel() override;

        // =============================================================================
        // Copy delete
        // =============================================================================
    public:
        HierarchyPanel(const HierarchyPanel&)            = delete;
        HierarchyPanel& operator=(const HierarchyPanel&) = delete;

        // =============================================================================
        // Override
        // =============================================================================
    public:
        //~Begin IEditorPanel interface
        /** No resource to acquire — the panel reads the world through the context. */
        void            Startup()               override {}

        /** Nothing the world's render depends on — the row list is built in Draw. */
        void            OnPreRender()           override {}

        /**
         * One ImGui::Selectable per entity, highlighted when it matches the current selection; a click
         * writes the selection. Empty states are explicit text, never a blank panel (L12).
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

        const OpaaxStringID m_PanelID{ OPAAX_ID("Hierarchy") };
        const OpaaxString   m_Title = m_PanelID.ToString();

        // One-shot: Draw() is per-frame, and BOTH empty states are silent — a clean log would otherwise be
        // indistinguishable from an empty panel (L15). Logs the SUCCESS branch once, then never again.
        bool m_bListLogged = false;
    };
}
