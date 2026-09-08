#include "Editor/Undo/InputUndoables.h"

#include "Editor/EditorContext.h"
#include "Editor/Resources/Types/Input/EditorInputActionDocument.h"
#include "Editor/Resources/Types/Input/EditorInputMappingContextDocument.h"

namespace Opaax::Editor
{
    namespace
    {
        /** The open action's data when it is the one InPath names, else null WITH A LINE SAYING SO. */
        InputActionData* TargetAction(EditorContext& InContext, const OpaaxString& InPath)
        {
            if (!InContext.InputActionDocument.IsOpen() || InContext.InputActionDocument.AbsPath() != InPath)
            {
                OPAAX_LOG(LogEditorInputActionDocument, Warn,
                          "Undo step skipped — its input action '{}' is not the open one", InPath.CStr());
                return nullptr;
            }

            return &InContext.InputActionDocument.GetMutableData();
        }

        /** The open context's data when it is the one InPath names, else null WITH A LINE SAYING SO. */
        InputMappingContextData* TargetMap(EditorContext& InContext, const OpaaxString& InPath)
        {
            if (!InContext.InputMapDocument.IsOpen() || InContext.InputMapDocument.AbsPath() != InPath)
            {
                OPAAX_LOG(LogEditorInputMapDocument, Warn,
                          "Undo step skipped — its input map '{}' is not the open one", InPath.CStr());
                return nullptr;
            }

            return &InContext.InputMapDocument.GetMutableData();
        }
    }

    // =============================================================================
    // InputActionEdit
    // =============================================================================
    void InputActionEdit::Undo(EditorContext& InContext)
    {
        if (InputActionData* lData = TargetAction(InContext, ActionPath))
        {
            *lData = Before;
        }
    }

    void InputActionEdit::Redo(EditorContext& InContext)
    {
        if (InputActionData* lData = TargetAction(InContext, ActionPath))
        {
            *lData = After;
        }
    }

    // =============================================================================
    // InputMappingsEdit
    // =============================================================================
    void InputMappingsEdit::Undo(EditorContext& InContext)
    {
        if (InputMappingContextData* lData = TargetMap(InContext, MapPath))
        {
            lData->Mappings = Before;
        }
    }

    void InputMappingsEdit::Redo(EditorContext& InContext)
    {
        if (InputMappingContextData* lData = TargetMap(InContext, MapPath))
        {
            lData->Mappings = After;
        }
    }

    // =============================================================================
    // InputMapPriorityEdit
    // =============================================================================
    void InputMapPriorityEdit::Undo(EditorContext& InContext)
    {
        if (InputMappingContextData* lData = TargetMap(InContext, MapPath))
        {
            lData->Priority = Before;
        }
    }

    void InputMapPriorityEdit::Redo(EditorContext& InContext)
    {
        if (InputMappingContextData* lData = TargetMap(InContext, MapPath))
        {
            lData->Priority = After;
        }
    }
}
