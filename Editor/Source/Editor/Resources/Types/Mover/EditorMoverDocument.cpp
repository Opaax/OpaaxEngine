#include "Editor/Resources/Types/Mover/EditorMoverDocument.h"

#include "Core/String/OpaaxPathString.h"
#include "Engine/Subsystems/Resources/Types/Mover/MoverFile.h"

namespace Opaax::Editor
{
    bool EditorMoverDocument::Open(const OpaaxString& InAbsPath)
    {
        MoverData lLoaded;

        // Into a LOCAL first: a file that fails to parse must not half-replace the mover already
        // being edited.
        if (!MoverFile::Load(InAbsPath, lLoaded))
        {
            return false;
        }

        m_AbsPath  = InAbsPath;
        m_Data     = Move(lLoaded);
        m_Baseline = MoverFile::Serialize(m_Data);

        OPAAX_LOG(LogEditorMoverDocument, Info, "Editing mover '{}' — {} mode name(s), default '{}'",
                  InAbsPath.CStr(), m_Data.EntryCount(),
                  m_Data.DefaultMode.IsValid() ? m_Data.DefaultMode.CStr() : "(none)");
        return true;
    }

    void EditorMoverDocument::Close()
    {
        m_AbsPath  = OpaaxString();
        m_Data     = MoverData{};
        m_Baseline = OpaaxString();
    }

    void EditorMoverDocument::MarkSaved()
    {
        m_Baseline = MoverFile::Serialize(m_Data);
    }

    bool EditorMoverDocument::IsDirty() const
    {
        return IsOpen() && MoverFile::Serialize(m_Data) != m_Baseline;
    }

    OpaaxString EditorMoverDocument::FileName() const
    {
        if (!IsOpen()) { return OpaaxString(); }

        return PathString::FileName(m_AbsPath).ToString();
    }
}
