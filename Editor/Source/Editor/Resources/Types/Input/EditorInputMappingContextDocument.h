#pragma once

#include "Application/Services/ILogger.h"
#include "Core/String/OpaaxString.hpp"
#include "Engine/Subsystems/Resources/Types/Input/InputMappingContextData.h"

namespace Opaax::Editor
{
    inline constexpr LogCategory LogEditorInputMapDocument{"EditorInputMapDocument"};

    // =============================================================================
    // EditorInputMappingContextDocument — WHICH `.opaaxinputmap` is open, its live data, and
    //   whether that data still matches what was last written. EditorMoverDocument's shape.
    //
    //   THIS IS THE FILE REBINDING EDITS. An action never changes when a key does — the mapping
    //   is what says which key reaches it, which is why the two are separate assets at all.
    // =============================================================================
    class EditorInputMappingContextDocument
    {
        // =============================================================================
        // Ctor
        // =============================================================================
    public:
        EditorInputMappingContextDocument() = default;

        EditorInputMappingContextDocument(const EditorInputMappingContextDocument&)            = delete;
        EditorInputMappingContextDocument& operator=(const EditorInputMappingContextDocument&) = delete;

        // =============================================================================
        // Functions
        // =============================================================================
    public:
        /** Read InAbsPath and make it the open context. A failure leaves the previous one open. */
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

        const InputMappingContextData& GetData() const noexcept { return m_Data; }

        /** The editable copy. Every mutation goes through a verb that also records an undo step. */
        InputMappingContextData& GetMutableData() noexcept { return m_Data; }

        /** Whether the data differs from what was last written. Recomputed, never cached. */
        bool IsDirty() const;

        // =============================================================================
        // Members
        // =============================================================================
    private:
        OpaaxString             m_AbsPath;
        InputMappingContextData m_Data;

        /** The serialized text as of the last Open/Save — what IsDirty compares against. */
        OpaaxString             m_Baseline;
    };
}
