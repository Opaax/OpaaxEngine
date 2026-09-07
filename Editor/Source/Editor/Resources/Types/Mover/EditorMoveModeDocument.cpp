#include "Editor/Resources/Types/Mover/EditorMoveModeDocument.h"

#include "Core/String/OpaaxPathString.h"
#include "Engine/Subsystems/Resources/Types/Mover/MoveModeFile.h"

namespace Opaax::Editor
{
    bool EditorMoveModeDocument::Open(const OpaaxString& InAbsPath)
    {
        MoveModeData lLoaded;

        // Into a LOCAL first: a file that fails to parse must not half-replace the tuning already
        // being edited.
        if (!MoveModeFile::Load(InAbsPath, lLoaded))
        {
            return false;
        }

        m_AbsPath  = InAbsPath;
        m_Data     = lLoaded;
        m_Baseline = MoveModeFile::Serialize(m_Data);

        OPAAX_LOG(LogEditorMoveModeDocument, Info, "Editing move mode '{}' — mode '{}', max speed {}",
                  InAbsPath.CStr(), m_Data.Mode.CStr(), m_Data.MaxSpeed);
        return true;
    }

    void EditorMoveModeDocument::Close()
    {
        m_AbsPath  = OpaaxString();
        m_Data     = MoveModeData{};
        m_Baseline = OpaaxString();
    }

    void EditorMoveModeDocument::MarkSaved()
    {
        m_Baseline = MoveModeFile::Serialize(m_Data);
    }

    bool EditorMoveModeDocument::IsDirty() const
    {
        return IsOpen() && MoveModeFile::Serialize(m_Data) != m_Baseline;
    }

    OpaaxString EditorMoveModeDocument::FileName() const
    {
        if (!IsOpen()) { return OpaaxString(); }

        return PathString::FileName(m_AbsPath).ToString();
    }
}
