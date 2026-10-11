#pragma once

#include "Core/Log/Logger.h"
#include "Core/String/OpaaxString.hpp"
#include "Animation/AnimationClipData.h"

namespace Opaax::Editor
{
    inline constexpr LogCategory LogEditorAnimationClipDocument{"EditorAnimationClipDocument"};

    // =============================================================================
    // EditorAnimationClipDocument — which .opaaxclip is open, its editable data, and whether it
    //   matches what was last written. Holds its own copy: the ResourceManager's copy is what the
    //   animator plays, and ClipOps::Save writes and reloads it.
    //   The dirty state is derived: IsDirty re-serializes and compares with the last Open/Save.
    // =============================================================================
    class EditorAnimationClipDocument
    {
        // =============================================================================
        // Ctor
        // =============================================================================
    public:
        EditorAnimationClipDocument() = default;

        EditorAnimationClipDocument(const EditorAnimationClipDocument&)            = delete;
        EditorAnimationClipDocument& operator=(const EditorAnimationClipDocument&) = delete;

        // =============================================================================
        // Functions
        // =============================================================================
    public:
        /**
         * Reads InAbsPath and makes it the open clip. A file that fails to load keeps the previous one open
         * and returns false.
         */
        bool Open(const OpaaxString& InAbsPath);

        /** Nothing open. Discards unsaved edits — the caller is what asks first. */
        void Close();

        /** Take the current data as the new baseline. Called after a successful Save. */
        void MarkSaved();

        // =============================================================================
        // Get
        // =============================================================================
    public:
        bool               IsOpen()  const noexcept { return !m_AbsPath.IsEmpty(); }
        const OpaaxString& AbsPath() const noexcept { return m_AbsPath; }

        /** Just the file name, for the panel title — "Hero_Run.opaaxclip". Empty when none is open. */
        OpaaxString FileName() const;

        const AnimationClipData& GetData() const noexcept { return m_Data; }

        /** The editable copy. Every change goes through an action that records an undo step. */
        AnimationClipData& GetMutableData() noexcept { return m_Data; }

        /** Whether the data differs from what was last written. Recomputed, never cached. */
        bool IsDirty() const;

        // =============================================================================
        // Members
        // =============================================================================
    private:
        OpaaxString       m_AbsPath;
        AnimationClipData m_Data;

        /** The serialized text as of the last Open/Save — what IsDirty compares against. */
        OpaaxString       m_Baseline;
    };
}
