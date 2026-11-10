#pragma once

#include <nlohmann/json.hpp>

#include "Core/String/OpaaxString.hpp"
#include "Editor/Clipboard/ClipboardPayload.hpp"

namespace Opaax::Editor
{
    class IEditorGui;

    // =============================================================================
    // EditorClipboard — copy and paste for every editor document, through the system clipboard (a copy
    //   can be pasted in another editor, or read in a text editor). Owned by EditorService, reached
    //   through EditorContext. Keeps the last text it put there, for a system with no clipboard (a
    //   virtual screen in CI).
    // =============================================================================
    class EditorClipboard
    {
    public:
        explicit EditorClipboard(IEditorGui& InGui) : m_Gui(InGui) {}

        /** Puts InData on the clipboard as a copy of InKind (ClipboardPayload). */
        void Copy(EClipboardKind InKind, const nlohmann::json& InData);

        /**
         * The data of the copy on the clipboard. Never throws.
         * @param OutError Why not (no copy from the editor, another kind)
         * @return False when the clipboard holds no copy of InKind
         */
        bool Read(EClipboardKind InKind, nlohmann::json& OutData, OpaaxString& OutError) const;

        /** Puts InText on the clipboard as it is. */
        void SetText(const OpaaxString& InText);

        /** The clipboard's text: the system's, else the last text put there. */
        OpaaxString GetText() const;

    private:
        IEditorGui& m_Gui;
        OpaaxString m_Last;
    };
}
