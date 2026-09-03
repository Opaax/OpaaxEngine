#include "Editor/EditorSpriteSheetDocument.h"

#include "Core/String/OpaaxPathString.h"
#include "Engine/Subsystems/Resources/Types/SpriteSheetFile.h"

namespace Opaax::Editor
{
    bool EditorSpriteSheetDocument::Open(const OpaaxString& InAbsPath)
    {
        SpriteSheetData lLoaded;

        // Into a LOCAL first: a file that fails to parse must not half-replace the sheet already
        // being edited, and SpriteSheetFile already leaves its out-parameter untouched on failure.
        if (!SpriteSheetFile::Load(InAbsPath, lLoaded))
        {
            return false;
        }

        m_AbsPath  = InAbsPath;
        m_Data     = Move(lLoaded);
        m_Baseline = SpriteSheetFile::Serialize(m_Data);

        OPAAX_LOG(LogEditorSpriteSheetDocument, Info, "Editing sheet '{}' — {} frame(s), default {}",
                  InAbsPath.CStr(), m_Data.FrameCount(), m_Data.DefaultFrame);
        return true;
    }

    void EditorSpriteSheetDocument::Close()
    {
        m_AbsPath  = OpaaxString();
        m_Data     = SpriteSheetData{};
        m_Baseline = OpaaxString();
    }

    void EditorSpriteSheetDocument::MarkSaved()
    {
        m_Baseline = SpriteSheetFile::Serialize(m_Data);
    }

    bool EditorSpriteSheetDocument::IsDirty() const
    {
        return IsOpen() && SpriteSheetFile::Serialize(m_Data) != m_Baseline;
    }

    OpaaxString EditorSpriteSheetDocument::FileName() const
    {
        if (!IsOpen()) { return OpaaxString(); }

        return PathString::FileName(m_AbsPath).ToString();
    }
}
