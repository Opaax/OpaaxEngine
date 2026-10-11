#pragma once

#include "Core/Log/Logger.h"
#include "Core/String/OpaaxString.hpp"
#include "Renderer/Text/FontFamilyData.h"

namespace Opaax::Editor
{
    inline constexpr LogCategory LogEditorFontFamilyDocument{"EditorFontFamilyDocument"};

    // =============================================================================
    // EditorFontFamilyDocument — the open .opaaxfont, its data, and whether it changed since the
    //   last save.
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
        /** Reads InAbsPath and makes it the open document. On failure the previous one stays open. */
        bool Open(const OpaaxString& InAbsPath);

        /** Closes the document. Discards unsaved edits (the caller asks first). */
        void Close();

        /** Takes the current data as the new baseline. Called after a successful Save. */
        void MarkSaved();

        // =============================================================================
        // Get
        // =============================================================================
    public:
        bool               IsOpen()  const noexcept { return !m_AbsPath.IsEmpty(); }
        const OpaaxString& AbsPath() const noexcept { return m_AbsPath; }

        /** The file name, for the panel title ("Roboto.opaaxfont"). Empty when none is open. */
        OpaaxString FileName() const;

        const FontFamilyData& GetData() const noexcept { return m_Data; }

        /** The editable copy. Every change goes through a verb that also records an undo step. */
        FontFamilyData& GetMutableData() noexcept { return m_Data; }

        /** Whether the data differs from what was last written. Recomputed each time. */
        bool IsDirty() const;

        // =============================================================================
        // Members
        // =============================================================================
    private:
        OpaaxString    m_AbsPath;
        FontFamilyData m_Data;

        /** The serialized text at the last Open/Save (what IsDirty compares against). */
        OpaaxString    m_Baseline;
    };
}
