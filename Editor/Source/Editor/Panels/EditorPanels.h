#pragma once

#include "Application/Services/ILogger.h"   // OPAAX_LOG_CATEGORY
#include "Core/OpaaxTypes.h"                // TUniquePtr, TDynArray, Uint64
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
    // EditorPanels — the live panels: every instance the registry described, plus whether each one
    //   is on screen. Owned by EditorService, referenced from EditorContext.
    //
    //   IT IS THE ONLY PLACE THAT OPENS A PANEL WINDOW. A panel emits widgets; the label, the
    //   first-use size and the close button are decided in Draw() below and emitted through
    //   IEditorGui — so visibility is one bool per panel that both the menu tick and the X write,
    //   and this file names no UI backend.
    //
    //   Construction is registration order, teardown is its reverse (LC3). The Viewport registers
    //   first (natives before modules, MR2), which is what keeps its render-target handshake
    //   independent of what a game module registers.
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

        // Owns panels through TUniquePtr (I6's corollary: an owner of a move-only member must say so).
        EditorPanels(const EditorPanels&)            = delete;
        EditorPanels& operator=(const EditorPanels&) = delete;

        // =============================================================================
        // Functions
        // =============================================================================
    public:
        /** Run every factory once, in registration order, and Startup each panel it produced. */
        void Build(const PanelRegistry& InRegistry, EditorContext& InContext);

        /** Every panel, hidden or not — a hidden panel still has to clear what DrawContents measures. */
        void OnPreRender();

        /**
         * Open each visible panel's window and draw its contents.
         *
         * Called by IEditorGui::Draw, which owns the whole UI pass — not by the composition root.
         * Takes the gui rather than the whole EditorContext: the window chrome is all this needs.
         */
        void Draw(IEditorGui& InGui);

        /** Shutdown + destroy in reverse construction order (LC3). Idempotent. */
        void Shutdown();

        void OnActiveWorldChanged(World* InOld, World* InNew);

        // =============================================================================
        // Get - Set
    public:
        /** @return false for an unknown id — a menu tick for a panel nobody registered reads "off". */
        bool IsVisible(OpaaxStringID InID) const noexcept;

        /**
         * THE one place a panel's visibility moves — the Window menu, the window's own close button
         * and anything later all arrive here, which is why the transition is logged here and nowhere
         * else. A no-op when already in that state; unknown id logs a Warn and does nothing.
         */
        void SetVisible(OpaaxStringID InID, bool bInVisible);

        /**
         * Which panel had keyboard focus when the last Draw ran, or an invalid id.
         *
         * Measured by the DRAW LOOP because that is the only place each panel's window is open —
         * the same shape the ViewportPanel's measured size has, and the reason an editor-wide
         * shortcut can mean different things in different panels without every panel owning a chord.
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

        /** The focused panel's id as of the last Draw. Invalid when the focus is elsewhere. */
        OpaaxStringID        m_Focused;
    };
}
