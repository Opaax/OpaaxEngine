#pragma once

#include "Core/Log/Logger.h"   // OPAAX_LOG_CATEGORY
#include "Core/OpaaxTypes.h"
#include "Editor/Panels/IEditorPanel.h"
#include "Editor/Panels/PanelDesc.h"

namespace Opaax
{
    class World;

    OPAAX_LOG_CATEGORY(EditorPanels);
}

namespace Opaax::Editor
{
    struct EditorContext;
    class IEditorGui;
    class PanelRegistry;

    // =============================================================================
    // EditorPanels — the live panels and whether each is visible. Owned by EditorService.
    //   The only place that opens panel windows (label, first-use size, close button), through
    //   IEditorGui. Created in registration order, destroyed in reverse.
    // =============================================================================
    class EditorPanels
    {
        // =============================================================================
        // Ctor - Dtor
        // =============================================================================
    public:
        EditorPanels()  = default;
        ~EditorPanels() = default;

        // =============================================================================
        // Copy - Move Delete
        // =============================================================================

        // Owns panels through TUniquePtr.
        EditorPanels(const EditorPanels&)            = delete;
        EditorPanels& operator=(const EditorPanels&) = delete;

        // =============================================================================
        // Functions
        // =============================================================================
    public:
        /** Runs every factory once, in registration order, and starts each panel. */
        void Build(const PanelRegistry& InRegistry, EditorContext& InContext);

        /** Every panel, hidden or not. */
        void OnPreRender();

        /**
         * Opens each visible panel's window and draws its contents. Called by IEditorGui::Draw.
         */
        void Draw(IEditorGui& InGui);

        /** Shuts down and destroys the panels in reverse order. Safe to call twice. */
        void Shutdown();

        void OnActiveWorldChanged(World* InOld, World* InNew);

        // =============================================================================
        // Get - Set
    public:
        /** @return False for an unknown id */
        bool IsVisible(OpaaxStringID InID) const noexcept;

        /**
         * The only place a panel's visibility changes (and is logged). Does nothing when already in that
         * state; an unknown id logs a warning.
         */
        void SetVisible(OpaaxStringID InID, bool bInVisible);

        /**
         * The panel that had keyboard focus during the last Draw, or an invalid id. Lets editor-wide
         * shortcuts act on the focused panel.
         */
        OpaaxStringID FocusedPanel() const noexcept { return m_Focused; }

        Uint64 Count() const noexcept { return static_cast<Uint64>(m_Panels.size()); }
        // End Get - Set
        // =============================================================================

        // =============================================================================
        // Members
        // =============================================================================
    private:
        struct LivePanel
        {
            PanelDesc                Desc;
            TUniquePtr<IEditorPanel> Panel;
            bool                     bVisible = true;
        };

        LivePanel*       Find(OpaaxStringID InID) noexcept;
        const LivePanel* Find(OpaaxStringID InID) const noexcept;

        TDynArray<LivePanel> m_Panels;

        /** The focused panel's id at the last Draw. Invalid when the focus is elsewhere. */
        OpaaxStringID        m_Focused;
    };
}
