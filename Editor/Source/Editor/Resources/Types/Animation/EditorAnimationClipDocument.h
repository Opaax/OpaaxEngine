#pragma once

#include "Application/Services/ILogger.h"
#include "Core/String/OpaaxString.hpp"
#include "Engine/Subsystems/Resources/Types/Animation/AnimationClipData.h"

namespace Opaax::Editor
{
    inline constexpr LogCategory LogEditorAnimationClipDocument{"EditorAnimationClipDocument"};

    // =============================================================================
    // EditorAnimationClipDocument — WHICH `.opaaxclip` is open, its live data, and whether that
    //   data still matches what was last written.
    //
    //   EditorSpriteSheetDocument's shape verbatim, including the part that matters: IT HOLDS THE
    //   DATA. The copy in the ResourceManager is what the ANIMATOR plays, so editing that one would
    //   change a running game mid-edit and lose the work on the next reload. A Save is what
    //   publishes it — and here that is a function (ClipOps::Save), not a sentence in a comment
    //   ([[L75]], which is the bug this whole pattern exists to have already fixed).
    //
    //   THE DIRTY MARKER IS DERIVED, never a flag: IsDirty re-serializes and compares against a
    //   baseline captured at Open/Save, so no mutation can forget to set it.
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
         * Read InAbsPath and make it the open clip.
         *
         * A file that fails to load leaves the previous one open and answers false — opening a
         * broken clip must not silently close the one being worked on.
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

        /** The editable copy. Every mutation goes through a verb that also records an undo step. */
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
