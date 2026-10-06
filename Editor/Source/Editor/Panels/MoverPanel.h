#pragma once

#include "Core/Log/Logger.h"
#include "Core/OpaaxTypes.h"
#include "Editor/Panels/IEditorPanel.h"

#include "Movement/Assets/MoverData.h"

namespace Opaax
{
    OPAAX_LOG_CATEGORY(MoverPanel);
}

namespace Opaax::Editor
{
    struct EditorContext;

    // =============================================================================
    // MoverPanel — the mover editor: one row per (name, tuning), like AnimationLibraryPanel.
    //   Entries are reflected; duplicate names are checked by MoverOps::CommitEntryEdit.
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
        /** Name, dirty marker, Save, and the default mode picker. */
        void DrawHeader(const MoverData& InData);

        /** One row per entry, with add, remove and reorder buttons. */
        void DrawEntryList(const MoverData& InData);

        /** The selected entry's two fields, with undo, committed through MoverOps. */
        void DrawSelectedEntry(MoverData& InData);

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
        // The open edit gesture: the data as it was when the first field became active
        // =============================================================================
        MoverEntry m_GestureBefore;
        Uint32     m_GestureIndex   = 0;
        bool       m_bGestureOpen   = false;
        bool       m_bWasItemActive = false;
    };
}
