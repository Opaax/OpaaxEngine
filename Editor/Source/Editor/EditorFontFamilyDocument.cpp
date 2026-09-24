#include "Editor/EditorFontFamilyDocument.h"

#include "Core/String/OpaaxPathString.h"
#include "Engine/Subsystems/Resources/Types/Font/FontFamilyFile.h"

namespace Opaax::Editor
{
    bool EditorFontFamilyDocument::Open(const OpaaxString& InAbsPath)
    {
        FontFamilyData lLoaded;

        // Into a LOCAL first: a file that fails to parse must not half-replace the family already
        // being edited.
        if (!FontFamilyFile::Load(InAbsPath, lLoaded))
        {
            return false;
        }

        m_AbsPath  = InAbsPath;
        m_Data     = Move(lLoaded);
        m_Baseline = FontFamilyFile::Serialize(m_Data);

        OPAAX_LOG(LogEditorFontFamilyDocument, Info, "Editing family '{}' — {} face(s)",
                  InAbsPath.CStr(), m_Data.EntryCount());
        return true;
    }

    void EditorFontFamilyDocument::Close()
    {
        m_AbsPath  = OpaaxString();
        m_Data     = FontFamilyData{};
        m_Baseline = OpaaxString();
    }

    void EditorFontFamilyDocument::MarkSaved()
    {
        m_Baseline = FontFamilyFile::Serialize(m_Data);
    }

    bool EditorFontFamilyDocument::IsDirty() const
    {
        return IsOpen() && FontFamilyFile::Serialize(m_Data) != m_Baseline;
    }

    OpaaxString EditorFontFamilyDocument::FileName() const
    {
        if (!IsOpen()) { return OpaaxString(); }

        return PathString::FileName(m_AbsPath).ToString();
    }
}
