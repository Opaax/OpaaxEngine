#pragma once

#include "Core/Log/Logger.h"
#include "Core/String/OpaaxString.hpp"
#include "Renderer/Textures/SpriteSheetData.h"

namespace Opaax::Editor
{
    inline constexpr LogCategory LogEditorSpriteSheetDocument{"EditorSpriteSheetDocument"};

    // =============================================================================
    // EditorSpriteSheetDocument — the open .opaaxsheet, its data, and whether it changed since the
    //   last save. Owns its data: the ResourceManager's copy is what the renderer draws, so it is not
    //   edited directly; Save publishes the changes. Dirty state is derived by comparing text.
    // =============================================================================
    class EditorSpriteSheetDocument
    {
        // =============================================================================
        // Ctor
        // =============================================================================
    public:
        EditorSpriteSheetDocument() = default;

        EditorSpriteSheetDocument(const EditorSpriteSheetDocument&)            = delete;
        EditorSpriteSheetDocument& operator=(const EditorSpriteSheetDocument&) = delete;

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

        /** The file name, for the panel title ("Hero.opaaxsheet"). Empty when none is open. */
        OpaaxString FileName() const;

        const SpriteSheetData& GetData() const noexcept { return m_Data; }

        /** The editable copy. Every change goes through a verb that also records an undo step. */
        SpriteSheetData& GetMutableData() noexcept { return m_Data; }

        /** Whether the data differs from what was last written. Recomputed each time. */
        bool IsDirty() const;

        // =============================================================================
        // Members
        // =============================================================================
    private:
        OpaaxString     m_AbsPath;
        SpriteSheetData m_Data;

        /** The serialized text at the last Open/Save (what IsDirty compares against). */
        OpaaxString     m_Baseline;
    };
}
