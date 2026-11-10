#include "Editor/Clipboard/EditorClipboard.h"

#include "Editor/UI/IEditorGui.h"

namespace Opaax::Editor
{
    void EditorClipboard::Copy(const EClipboardKind InKind, const nlohmann::json& InData)
    {
        SetText(ClipboardPayload::Make(InKind, InData));
    }

    bool EditorClipboard::Read(const EClipboardKind InKind, nlohmann::json& OutData, OpaaxString& OutError) const
    {
        return ClipboardPayload::Parse(GetText(), InKind, OutData, OutError);
    }

    void EditorClipboard::SetText(const OpaaxString& InText)
    {
        m_Last = InText;
        m_Gui.SetClipboardText(InText);
    }

    OpaaxString EditorClipboard::GetText() const
    {
        OpaaxString lSystem = m_Gui.GetClipboardText();
        return lSystem.IsEmpty() ? m_Last : lSystem;
    }
}
