#pragma once

#include <nlohmann/json.hpp>

#include "Core/Log/Logger.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"
#include "Core/String/OpaaxStringID.hpp"

namespace Opaax
{
    class DataAssetTypeRegistry;
    class IDataAssetObject;
    class IDataAssetTypeEntry;
}

namespace Opaax::Editor
{
    inline constexpr LogCategory LogEditorDataAssetDocument{"EditorDataAssetDocument"};

    // =============================================================================
    // EditorDataAssetDocument — the open .opaaxdata, whatever its type. Holds its own copy (the
    //   ResourceManager's one is what a running game reads). A type nobody registered is kept as raw
    //   JSON: shown, not editable, and never rewritten.
    // =============================================================================
    class EditorDataAssetDocument
    {
    public:
        EditorDataAssetDocument();
        ~EditorDataAssetDocument();

        EditorDataAssetDocument(const EditorDataAssetDocument&)            = delete;
        EditorDataAssetDocument& operator=(const EditorDataAssetDocument&) = delete;

        // =============================================================================
        // Functions
        // =============================================================================
    public:
        /** Reads InAbsPath and makes it the open asset. A failure leaves the previous one open. */
        bool Open(const OpaaxString& InAbsPath, const DataAssetTypeRegistry& InTypes);

        /** Nothing open. Discards unsaved edits — the caller is what asks first. */
        void Close();

        /** Takes the current data as the new baseline. Called after a successful Save. */
        void MarkSaved();

        /** Replaces the value (undo/redo). False if the type is unknown or the data does not read. */
        bool SetFromJson(const nlohmann::json& InData);

        // =============================================================================
        // Get
        // =============================================================================
    public:
        bool               IsOpen()  const noexcept { return !m_AbsPath.IsEmpty(); }
        const OpaaxString& AbsPath() const noexcept { return m_AbsPath; }

        /** Just the file name, for the panel title. Empty when none is open. */
        OpaaxString FileName() const;

        OpaaxStringID TypeName() const noexcept { return m_Type; }

        /** False when the file names a type no module registered. */
        bool IsTypeKnown() const noexcept { return m_Object != nullptr; }

        /** The editable value; null when the type is unknown. */
        IDataAssetObject* GetObject() noexcept { return m_Object.get(); }

        /** The value as JSON (the raw data when the type is unknown). */
        nlohmann::json CurrentJson() const;

        /** The exact text Save would write. */
        OpaaxString Serialize() const;

        /** Whether the data differs from what was last written. Recomputed, never cached. */
        bool IsDirty() const;

        // =============================================================================
        // Members
        // =============================================================================
    private:
        OpaaxString                  m_AbsPath;
        OpaaxStringID                m_Type;
        const IDataAssetTypeEntry*   m_Entry = nullptr;   // owned by the engine's registry
        TUniquePtr<IDataAssetObject> m_Object;
        nlohmann::json               m_RawData;           // kept for an unknown type

        /** The serialized text as of the last Open/Save — what IsDirty compares against. */
        OpaaxString m_Baseline;
    };
}
