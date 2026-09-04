#include "Editor/Resources/Types/Animation/EditorAnimationClipDocument.h"

#include "Core/String/OpaaxPathString.h"
#include "Engine/Subsystems/Resources/Types/Animation/AnimationClipFile.h"

namespace Opaax::Editor
{
    bool EditorAnimationClipDocument::Open(const OpaaxString& InAbsPath)
    {
        AnimationClipData lLoaded;

        // Into a LOCAL first: a file that fails to parse must not half-replace the clip already
        // being edited, and AnimationClipFile already leaves its out-parameter untouched on failure.
        if (!AnimationClipFile::Load(InAbsPath, lLoaded))
        {
            return false;
        }

        m_AbsPath  = InAbsPath;
        m_Data     = Move(lLoaded);
        m_Baseline = AnimationClipFile::Serialize(m_Data);

        OPAAX_LOG(LogEditorAnimationClipDocument, Info, "Editing clip '{}' — {} step(s) @ {} fps, {}",
                  InAbsPath.CStr(), m_Data.StepCount(), m_Data.Fps, ToString(m_Data.PlayMode));
        return true;
    }

    void EditorAnimationClipDocument::Close()
    {
        m_AbsPath  = OpaaxString();
        m_Data     = AnimationClipData{};
        m_Baseline = OpaaxString();
    }

    void EditorAnimationClipDocument::MarkSaved()
    {
        m_Baseline = AnimationClipFile::Serialize(m_Data);
    }

    bool EditorAnimationClipDocument::IsDirty() const
    {
        return IsOpen() && AnimationClipFile::Serialize(m_Data) != m_Baseline;
    }

    OpaaxString EditorAnimationClipDocument::FileName() const
    {
        if (!IsOpen()) { return OpaaxString(); }

        return PathString::FileName(m_AbsPath).ToString();
    }
}
