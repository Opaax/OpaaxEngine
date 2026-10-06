#include "Editor/Resources/Types/Input/EditorInputActionDocument.h"

#include "Core/String/OpaaxPathString.h"
#include "Input/Assets/InputActionFile.h"

namespace Opaax::Editor
{
    bool EditorInputActionDocument::Open(const OpaaxString& InAbsPath)
    {
        InputActionData lLoaded;

        // Loaded into a local first, so a file that fails to parse does not half-replace the open one.
        if (!InputActionFile::Load(InAbsPath, lLoaded))
        {
            return false;
        }

        m_AbsPath  = InAbsPath;
        m_Data     = lLoaded;
        m_Baseline = InputActionFile::Serialize(m_Data);

        OPAAX_LOG(LogEditorInputActionDocument, Info, "Editing input action '{}' — '{}' ({}), {} modifier(s)",
                  InAbsPath.CStr(), m_Data.Name, ToString(m_Data.ValueType),
                  static_cast<Uint64>(m_Data.Modifiers.size()));
        return true;
    }

    void EditorInputActionDocument::Close()
    {
        m_AbsPath  = OpaaxString();
        m_Data     = InputActionData{};
        m_Baseline = OpaaxString();
    }

    void EditorInputActionDocument::MarkSaved()
    {
        m_Baseline = InputActionFile::Serialize(m_Data);
    }

    bool EditorInputActionDocument::IsDirty() const
    {
        return IsOpen() && InputActionFile::Serialize(m_Data) != m_Baseline;
    }

    OpaaxString EditorInputActionDocument::FileName() const
    {
        if (!IsOpen()) { return OpaaxString(); }

        return PathString::FileName(m_AbsPath).ToString();
    }
}
