#pragma once

#include "Application/Services/ILogger.h"
#include "Core/OpaaxTypes.h"
#include "Editor/Panels/IEditorPanel.h"

#include "Engine/Subsystems/Resources/Types/Animation/AnimationLibraryData.h"   // the gesture caches an entry

namespace Opaax
{
    OPAAX_LOG_CATEGORY(AnimationLibraryPanel);
}

namespace Opaax::Editor
{
    struct EditorContext;

    // =============================================================================
    // AnimationLibraryPanel — the library EDITOR: a name, a clip, one row each.
    //
    //   The smallest of the document panels, and deliberately so: a library is an ALIAS TABLE, so
    //   there is nothing to draw but rows. No canvas, no preview — the clip panel owns those.
    //
    //   An entry is CReflected, so DrawProperties gives the name field AND the typed drop target
    //   with no drawer written here. What it cannot give is judgement about the REST of the
    //   library — a duplicate name makes one clip unreachable — so the gesture closes through
    //   LibraryOps::CommitEntryEdit, which is where that policy lives.
    // =============================================================================
    class AnimationLibraryPanel final : public IEditorPanel
    {
        // =============================================================================
        // Statics
        // =============================================================================
    public:
        OPAAX_EDITOR_PANEL_NAME(Animation Library);

        // =============================================================================
        // Ctor - Dtor
        // =============================================================================
    public:
        explicit AnimationLibraryPanel(EditorContext& InContext);
        ~AnimationLibraryPanel() override;

        // =============================================================================
        // Copy delete
        // =============================================================================
        AnimationLibraryPanel(const AnimationLibraryPanel&)            = delete;
        AnimationLibraryPanel& operator=(const AnimationLibraryPanel&) = delete;

        // =============================================================================
        // Functions
        // =============================================================================
    private:
        /** Name, dirty marker, Save, and the default-clip picker. */
        void DrawHeader(const AnimationLibraryData& InData);

        /** One selectable row per entry, and the buttons that add, remove and reorder them. */
        void DrawEntryList(const AnimationLibraryData& InData);

        /** The selected entry's two fields, bracketed for undo and committed through LibraryOps. */
        void DrawSelectedEntry(AnimationLibraryData& InData);

        // =============================================================================
        // Override
        // =============================================================================
    public:
        //~Begin IEditorPanel interface
        /** Nothing to acquire — a library holds paths, and this panel resolves none of them. */
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
        AnimationLibraryEntry m_GestureBefore;
        Uint32                m_GestureIndex   = 0;
        bool                  m_bGestureOpen   = false;
        bool                  m_bWasItemActive = false;
    };
}
