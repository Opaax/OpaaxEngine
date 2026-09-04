#pragma once

#include "Application/Services/ILogger.h"
#include "Core/String/OpaaxString.hpp"
#include "Engine/Subsystems/Resources/Types/FontFamilyData.h"

namespace Opaax::Editor
{
    inline constexpr LogCategory LogEditorFontFamilyDocument{"EditorFontFamilyDocument"};

    // =============================================================================
    // EditorFontFamilyDocument — WHICH `.opaaxfont` is open, its live data, and whether that data
    //   still matches what was last written.
    //
    //   The sixth document of this exact shape (map, level, sheet, clip, library, family) and the
    //   closest to the library's: a family is an alias table, so there is no canvas — but its table
    //   is a MATRIX, not a list, which is the panel's problem rather than this one's.
    // =============================================================================
    class EditorFontFamilyDocument
    {
        // =============================================================================
        // Ctor
        // =============================================================================
    public:
        EditorFontFamilyDocument() = default;

        EditorFontFamilyDocument(const EditorFontFamilyDocument&)            = delete;
        EditorFontFamilyDocument& operator=(const EditorFontFamilyDocument&) = delete;

        // =============================================================================
        // Functions
        // =============================================================================
    public:
        /** Read InAbsPath and make it the open family. A failure leaves the previous one open. */
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

        /** Just the file name, for the panel title — "Roboto.opaaxfont". Empty when none is open. */
        OpaaxString FileName() const;

        const FontFamilyData& GetData() const noexcept { return m_Data; }

        /** The editable copy. Every mutation goes through a verb that also records an undo step. */
        FontFamilyData& GetMutableData() noexcept { return m_Data; }

        /** Whether the data differs from what was last written. Recomputed, never cached. */
        bool IsDirty() const;

        // =============================================================================
        // Members
        // =============================================================================
    private:
        OpaaxString    m_AbsPath;
        FontFamilyData m_Data;

        /** The serialized text as of the last Open/Save — what IsDirty compares against. */
        OpaaxString    m_Baseline;
    };
}
