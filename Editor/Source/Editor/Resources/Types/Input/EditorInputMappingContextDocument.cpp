#include "Editor/Resources/Types/Input/EditorInputMappingContextDocument.h"

#include "Core/String/OpaaxPathString.h"
#include "Engine/Subsystems/Resources/Types/Input/InputMappingContextFile.h"

namespace Opaax::Editor
{
    bool EditorInputMappingContextDocument::Open(const OpaaxString& InAbsPath)
    {
        InputMappingContextData lLoaded;

        // Loaded into a local first, so a file that fails to parse does not half-replace the open one.
        if (!InputMappingContextFile::Load(InAbsPath, lLoaded))
        {
            return false;
        }

        m_AbsPath  = InAbsPath;
        m_Data     = lLoaded;
        m_Baseline = InputMappingContextFile::Serialize(m_Data);

        OPAAX_LOG(LogEditorInputMapDocument, Info, "Editing input map '{}' — {} mapping(s) at priority {}",
                  InAbsPath.CStr(), m_Data.EntryCount(), m_Data.Priority);
        return true;
    }

    void EditorInputMappingContextDocument::Close()
    {
        m_AbsPath  = OpaaxString();
        m_Data     = InputMappingContextData{};
        m_Baseline = OpaaxString();
    }

    void EditorInputMappingContextDocument::MarkSaved()
    {
        m_Baseline = InputMappingContextFile::Serialize(m_Data);
    }

    bool EditorInputMappingContextDocument::IsDirty() const
    {
        return IsOpen() && InputMappingContextFile::Serialize(m_Data) != m_Baseline;
    }

    OpaaxString EditorInputMappingContextDocument::FileName() const
    {
        if (!IsOpen()) { return OpaaxString(); }

        return PathString::FileName(m_AbsPath).ToString();
    }
}
