#include "Editor/Resources/Types/DataAsset/EditorDataAssetDocument.h"

#include "Core/String/OpaaxPathString.h"
#include "Resources/DataAsset/DataAssetFile.h"
#include "Resources/DataAsset/DataAssetTypeRegistry.h"

namespace Opaax::Editor
{
    EditorDataAssetDocument::EditorDataAssetDocument()  = default;
    EditorDataAssetDocument::~EditorDataAssetDocument() = default;

    bool EditorDataAssetDocument::Open(const OpaaxString& InAbsPath, const DataAssetTypeRegistry& InTypes)
    {
        DataAssetFile::Contents lContents;
        if (!DataAssetFile::Load(InAbsPath, lContents))
        {
            return false;   // DataAssetFile logged why
        }

        const IDataAssetTypeEntry*   lEntry = InTypes.FindByName(lContents.Type);
        TUniquePtr<IDataAssetObject> lObject;

        if (lEntry == nullptr)
        {
            OPAAX_LOG(LogEditorDataAssetDocument, Warn,
                      "'{}' holds a '{}', which no module registered — shown read-only", InAbsPath.CStr(),
                      lContents.Type.CStr());
        }
        else
        {
            OpaaxString lError;
            lObject = lEntry->CreateFromJson(lContents.Data, &lError);

            if (lObject == nullptr)
            {
                OPAAX_LOG(LogEditorDataAssetDocument, Error, "'{}' has an unreadable value: {}", InAbsPath.CStr(),
                          lError.CStr());
                return false;
            }
        }

        m_AbsPath = InAbsPath;
        m_Type    = lContents.Type;
        m_Entry   = lEntry;
        m_Object  = Move(lObject);
        m_RawData = Move(lContents.Data);

        MarkSaved();
        return true;
    }

    void EditorDataAssetDocument::Close()
    {
        m_AbsPath  = OpaaxString();
        m_Type     = OpaaxStringID();
        m_Entry    = nullptr;
        m_Object.reset();
        m_RawData  = nlohmann::json::object();
        m_Baseline = OpaaxString();
    }

    void EditorDataAssetDocument::MarkSaved()
    {
        m_Baseline = Serialize();
    }

    bool EditorDataAssetDocument::SetFromJson(const nlohmann::json& InData)
    {
        if (m_Entry == nullptr)
        {
            return false;
        }

        TUniquePtr<IDataAssetObject> lObject = m_Entry->CreateFromJson(InData, nullptr);
        if (lObject == nullptr)
        {
            return false;
        }

        m_Object = Move(lObject);
        return true;
    }

    OpaaxString EditorDataAssetDocument::FileName() const
    {
        return IsOpen() ? PathString::FileName(m_AbsPath).ToString() : OpaaxString();
    }

    nlohmann::json EditorDataAssetDocument::CurrentJson() const
    {
        return m_Object != nullptr ? m_Object->ToJson() : m_RawData;
    }

    OpaaxString EditorDataAssetDocument::Serialize() const
    {
        return DataAssetFile::Serialize(m_Type, CurrentJson());
    }

    bool EditorDataAssetDocument::IsDirty() const
    {
        return IsOpen() && Serialize() != m_Baseline;
    }
}
