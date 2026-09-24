#pragma once

#include "Application/Services/ILogger.h"
#include "Core/OpaaxTypes.h"
#include "Editor/Panels/IEditorPanel.h"

#include "Engine/Subsystems/Resources/Types/Mover/MoverData.h"   // the gesture caches an entry

namespace Opaax
{
    OPAAX_LOG_CATEGORY(MoverPanel);
}

namespace Opaax::Editor
{
    struct EditorContext;

    // =============================================================================
    // MoverPanel — the mover EDITOR: a name, a tuning, one row each.
    //
    //   AnimationLibraryPanel's shape exactly, because a mover IS a library: an ALIAS TABLE, so
    //   there is nothing to draw but rows. No canvas, no preview — the tuning has its own panel.
    //
    //   An entry is CReflected, so DrawProperties gives the name field AND the typed drop target
    //   with no drawer written here. What it cannot give is judgement about the REST of the mover —
    //   a duplicate name makes one mode unreachable — so the gesture closes through
    //   MoverOps::CommitEntryEdit, which is where that policy lives.
    // =============================================================================
    class MoverPanel final : public IEditorPanel
    {
        // =============================================================================
        // Statics
        // =============================================================================
    public:
        OPAAX_EDITOR_PANEL_NAME(Mover);

        // =============================================================================
        // Ctor - Dtor
        // =============================================================================
    public:
        explicit MoverPanel(EditorContext& InContext);
        ~MoverPanel() override;

        // =============================================================================
        // Copy delete
        // =============================================================================
        MoverPanel(const MoverPanel&)            = delete;
        MoverPanel& operator=(const MoverPanel&) = delete;

        // =============================================================================
        // Functions
        // =============================================================================
    private:
        /** Name, dirty marker, Save, and the default-mode picker. */
        void DrawHeader(const MoverData& InData);

        /** One selectable row per entry, and the buttons that add, remove and reorder them. */
        void DrawEntryList(const MoverData& InData);

        /** The selected entry's two fields, bracketed for undo and committed through MoverOps. */
        void DrawSelectedEntry(MoverData& InData);

        // =============================================================================
        // Override
        // =============================================================================
    public:
        //~Begin IEditorPanel interface
        /** Nothing to acquire — a mover holds paths, and this panel resolves none of them. */
        void Startup()     override {}

        /** Nothing the world's render depends on. */
        void OnPreRender() override {}

        void DrawContents() override;

        /** Nothing claimed, so nothing to release. */
        void Shutdown()    override {}

        PanelWindowStyle GetWindowStyle() const override { return { { 420.f, 340.f } }; }
        //~End IEditorPanel interface

        // =============================================================================
        // Members
        // =============================================================================
    private:
        EditorContext& m_Context;

        /** Which entry the list highlights. -1 = none. Presentation, not document state. */
        Int32 m_Selected = -1;

        // =============================================================================
        // The open edit gesture — the entry as it was when the first field went active
        // =============================================================================
        MoverEntry m_GestureBefore;
        Uint32     m_GestureIndex   = 0;
        bool       m_bGestureOpen   = false;
        bool       m_bWasItemActive = false;
    };
}
