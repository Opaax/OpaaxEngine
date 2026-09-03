#include "Editor/EditorAnimationLibraryDocument.h"

#include "Core/String/OpaaxPathString.h"
#include "Engine/Subsystems/Resources/Types/AnimationLibraryFile.h"

namespace Opaax::Editor
{
    bool EditorAnimationLibraryDocument::Open(const OpaaxString& InAbsPath)
    {
        AnimationLibraryData lLoaded;

        // Into a LOCAL first: a file that fails to parse must not half-replace the library already
        // being edited.
        if (!AnimationLibraryFile::Load(InAbsPath, lLoaded))
        {
            return false;
        }

        m_AbsPath  = InAbsPath;
        m_Data     = Move(lLoaded);
        m_Baseline = AnimationLibraryFile::Serialize(m_Data);

        OPAAX_LOG(LogEditorAnimationLibraryDocument, Info, "Editing library '{}' — {} clip name(s), default '{}'",
                  InAbsPath.CStr(), m_Data.EntryCount(),
                  m_Data.DefaultClip.IsValid() ? m_Data.DefaultClip.CStr() : "(none)");
        return true;
    }

    void EditorAnimationLibraryDocument::Close()
    {
        m_AbsPath  = OpaaxString();
        m_Data     = AnimationLibraryData{};
        m_Baseline = OpaaxString();
    }

    void EditorAnimationLibraryDocument::MarkSaved()
    {
        m_Baseline = AnimationLibraryFile::Serialize(m_Data);
    }

    bool EditorAnimationLibraryDocument::IsDirty() const
    {
        return IsOpen() && AnimationLibraryFile::Serialize(m_Data) != m_Baseline;
    }

    OpaaxString EditorAnimationLibraryDocument::FileName() const
    {
        if (!IsOpen()) { return OpaaxString(); }

        return PathString::FileName(m_AbsPath).ToString();
    }
}
