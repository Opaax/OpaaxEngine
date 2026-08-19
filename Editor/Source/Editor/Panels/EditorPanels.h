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
    class PanelRegistry;

    // =============================================================================
    // EditorPanels — the live panels: every instance the registry described, plus whether each one
    //   is on screen. Owned by EditorService, referenced from EditorContext.
    //
    //   IT IS THE ONLY PLACE THAT SPEAKS ImGui WINDOW. A panel emits widgets; the Begin/End pair,
    //   the label, the first-use size and the close button all live in Draw() below — so visibility
    //   is one bool per panel that both the menu tick and the window's X read and write.
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

        /** Open each visible panel's window and draw its contents. */
        void Draw();

        /** Shutdown + destroy in reverse construction order (LC3). Idempotent. */
        void Shutdown();

        void OnActiveWorldChanged(World* InOld, World* InNew);

        // =============================================================================
        // Get - Set
    public:
        /** @return false for an unknown id — a menu tick for a panel nobody registered reads "off". */
        bool IsVisible(OpaaxStringID InID) const noexcept;

        /** Unknown id logs a Warn and does nothing. */
        void SetVisible(OpaaxStringID InID, bool bInVisible);

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
    };
}
