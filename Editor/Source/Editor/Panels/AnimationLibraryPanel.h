#pragma once

#include "Core/Log/Logger.h"
#include "Core/OpaaxTypes.h"
#include "Editor/Panels/IEditorPanel.h"

#include "Animation/AnimationLibraryData.h"

namespace Opaax
{
    OPAAX_LOG_CATEGORY(AnimationLibraryPanel);
}

namespace Opaax::Editor
{
    struct EditorContext;

    // =============================================================================
    // AnimationLibraryPanel — the library editor: one row per (name, clip).
    //   Entries are reflected, so DrawProperties gives the fields. Duplicate names are checked by
    //   LibraryOps::CommitEntryEdit when an edit closes.
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
        /** Name, dirty marker, Save, and the default clip picker. */
        void DrawHeader(const AnimationLibraryData& InData);

        /** One row per entry, with add, remove and reorder buttons. */
        void DrawEntryList(const AnimationLibraryData& InData);

        /** The selected entry's two fields, with undo, committed through LibraryOps. */
        void DrawSelectedEntry(AnimationLibraryData& InData);

        // =============================================================================
        // Override
        // =============================================================================
    public:
        //~Begin IEditorPanel interface
        /** Nothing to acquire. */
        void Startup()     override {}

        /** Nothing the world render depends on. */
        void OnPreRender() override {}

        void DrawContents() override;

        /** Nothing to release. */
        void Shutdown()    override {}

        PanelWindowStyle GetWindowStyle() const override { return { { 420.f, 340.f } }; }
        //~End IEditorPanel interface

        // =============================================================================
        // Members
        // =============================================================================
    private:
        EditorContext& m_Context;

        /** Highlighted entry. -1 = none. */
        Int32 m_Selected = -1;

        // =============================================================================
        // The open edit gesture: the entry as it was when the first field became active
        // =============================================================================
        AnimationLibraryEntry m_GestureBefore;
        Uint32                m_GestureIndex   = 0;
        bool                  m_bGestureOpen   = false;
        bool                  m_bWasItemActive = false;
    };
}
