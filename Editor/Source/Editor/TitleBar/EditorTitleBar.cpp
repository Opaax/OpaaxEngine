#include "Editor/TitleBar/EditorTitleBar.h"

#include "Window/Window.h"
#include "Editor/Commands/EditorCommandRegistry.h"
#include "Editor/Commands/EditorNativeCommandsTags.hpp"
#include "Editor/EditorContext.h"
#include "Editor/Extensions/EditorExtensionRegistrar.h"
#include "Editor/TitleBar/TitleBarRegistry.h"
#include "Editor/UI/IEditorGui.h"

namespace Opaax::Editor
{
    void EditorTitleBar::Draw(EditorContext& InContext, IEditorGui& InGui) const
    {
        // --- Registered content, in bar order --------------------------------------------------
        if (m_Registry != nullptr)
        {
            for (const TUniquePtr<EditorTitleBarCategory>& lCategory : m_Registry->Categories())
            {
                lCategory->Draw(InContext);
            }
        }

        Window&                      lWindow   = InContext.MainWindow;
        const EditorCommandRegistry& lCommands = InContext.Extensions.Commands();
        const bool                   lMaximized = lWindow.IsMaximized();

        // --- The bar's own furniture, which nobody registers ------------------------------------
        // The drag takes the slack: whatever the menus and the three buttons leave. Claiming it in
        // this order is what keeps a menu click from being swallowed by it.
        const TitleBarDrag lDrag = InGui.TitleBarDragRegion(3);

        // A maximized window is not dragged — Windows restores-and-follows, polish this does not
        // have yet (the Win32 frame would give it).
        if (!lMaximized && (lDrag.Delta.x != 0.f || lDrag.Delta.y != 0.f))
        {
            Int32 lPosX = 0;
            Int32 lPosY = 0;
            lWindow.GetPosition(lPosX, lPosY);

            lWindow.SetPosition(lPosX + static_cast<Int32>(lDrag.Delta.x),
                                lPosY + static_cast<Int32>(lDrag.Delta.y));
        }

        if (lDrag.bDoubleClicked)
        {
            lCommands.Execute(Tags::EDITOR_COMMAND_TOGGLE_MAXIMIZE_WINDOW, InContext);
        }

        if (InGui.TitleBarButton(EWindowButtonKind::Minimize))
        {
            lCommands.Execute(Tags::EDITOR_COMMAND_MINIMIZE_WINDOW, InContext);
        }

        // The GLYPH follows the state, which is why the kind is decided here and not in the backend.
        if (InGui.TitleBarButton(lMaximized ? EWindowButtonKind::Restore : EWindowButtonKind::Maximize))
        {
            lCommands.Execute(Tags::EDITOR_COMMAND_TOGGLE_MAXIMIZE_WINDOW, InContext);
        }

        // The SAME verb File/Exit runs — the X and the menu entry are one verb, not two.
        if (InGui.TitleBarButton(EWindowButtonKind::Close))
        {
            lCommands.Execute(Tags::EDITOR_COMMAND_QUIT, InContext);
        }
    }
}
