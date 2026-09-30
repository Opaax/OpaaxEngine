#pragma once

#include "Core/Log/Logger.h"
#include "Core/String/OpaaxString.hpp"
#include "Engine/Subsystems/Resources/Types/Input/InputActionData.h"

namespace Opaax::Editor
{
    inline constexpr LogCategory LogEditorInputActionDocument{"EditorInputActionDocument"};

    // =============================================================================
    // EditorInputActionDocument — which .opaaxaction is open, its editable data, and whether it
    //   matches what was last written. An action names no keys (a mapping context does).
    // =============================================================================
    class EditorInputActionDocument
    {
        // =============================================================================
        // Ctor
        // =============================================================================
    public:
        EditorInputActionDocument() = default;

        EditorInputActionDocument(const EditorInputActionDocument&)            = delete;
        EditorInputActionDocument& operator=(const EditorInputActionDocument&) = delete;

        // =============================================================================
        // Functions
        // =============================================================================
    public:
        /** Read InAbsPath and make it the open action. A failure leaves the previous one open. */
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

        const InputActionData& GetData() const noexcept { return m_Data; }

        /** The editable copy. Every change goes through an action that records an undo step. */
        InputActionData& GetMutableData() noexcept { return m_Data; }

        /** Whether the data differs from what was last written. Recomputed, never cached. */
        bool IsDirty() const;

        // =============================================================================
        // Members
        // =============================================================================
    private:
        OpaaxString     m_AbsPath;
        InputActionData m_Data;

        /** The serialized text as of the last Open/Save — what IsDirty compares against. */
        OpaaxString     m_Baseline;
    };
}
