#pragma once

#include "Application/Services/ILogger.h"
#include "Core/String/OpaaxString.hpp"
#include "Engine/Subsystems/Resources/Types/Mover/MoveModeData.h"

namespace Opaax::Editor
{
    inline constexpr LogCategory LogEditorMoveModeDocument{"EditorMoveModeDocument"};

    // =============================================================================
    // EditorMoveModeDocument — WHICH `.opaaxmovemode` is open, its live data, and whether that
    //   data still matches what was last written.
    //
    //   The smallest document of the family: a tuning has no rows, no canvas and no preview — it
    //   is a flat set of knobs, so the panel is a property fold over one struct. It is still a
    //   DOCUMENT rather than a preview, because it is edited and saved.
    // =============================================================================
    class EditorMoveModeDocument
    {
        // =============================================================================
        // Ctor
        // =============================================================================
    public:
        EditorMoveModeDocument() = default;

        EditorMoveModeDocument(const EditorMoveModeDocument&)            = delete;
        EditorMoveModeDocument& operator=(const EditorMoveModeDocument&) = delete;

        // =============================================================================
        // Functions
        // =============================================================================
    public:
        /** Read InAbsPath and make it the open tuning. A failure leaves the previous one open. */
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

        /** Just the file name, for the panel title. Empty when none is open. */
        OpaaxString FileName() const;

        const MoveModeData& GetData() const noexcept { return m_Data; }

        /** The editable copy. Every mutation goes through a verb that also records an undo step. */
        MoveModeData& GetMutableData() noexcept { return m_Data; }

        /** Whether the data differs from what was last written. Recomputed, never cached. */
        bool IsDirty() const;

        // =============================================================================
        // Members
        // =============================================================================
    private:
        OpaaxString  m_AbsPath;
        MoveModeData m_Data;

        /** The serialized text as of the last Open/Save — what IsDirty compares against. */
        OpaaxString  m_Baseline;
    };
}
