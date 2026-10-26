#include "Editor/Undo/UICanvasUndoables.h"

#include "Application/OpaaxApplication.h"
#include "Application/Services/IEngine.h"
#include "Editor/EditorContext.h"
#include "Editor/EditorUICanvasDocument.h"
#include "Engine/Registries/EngineRegistries.h"

namespace Opaax::Editor
{
    namespace
    {
        /** Rebuilds the open canvas from InText, only if it is the one InPath names. */
        void RestoreInto(EditorContext& InContext, const OpaaxString& InPath, const OpaaxString& InText)
        {
            if (!InContext.UICanvasDocument.IsOpen() || InContext.UICanvasDocument.AbsPath() != InPath)
            {
                OPAAX_LOG(LogEditorUICanvasDocument, Warn,
                          "Undo step skipped — its canvas '{}' is not the open one", InPath.CStr());
                return;
            }

            const ObjectClassRegistry& lClasses =
                OpaaxApplication::GetAppService<IEngine>().GetRegistries().ObjectClasses();

            InContext.UICanvasDocument.RestoreFrom(InText, lClasses);
        }
    }

    void UITreeEdit::Undo(EditorContext& InContext)
    {
        RestoreInto(InContext, CanvasPath, Before);
    }

    void UITreeEdit::Redo(EditorContext& InContext)
    {
        RestoreInto(InContext, CanvasPath, After);
    }
}
