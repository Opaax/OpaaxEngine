#include "Editor/Undo/AnimationLibraryUndoables.h"

#include "Editor/EditorAnimationLibraryDocument.h"
#include "Editor/EditorContext.h"

namespace Opaax::Editor
{
    namespace
    {
        /** The open library's data when it is the one InPath names, else null WITH A LINE SAYING SO. */
        AnimationLibraryData* TargetLibrary(EditorContext& InContext, const OpaaxString& InPath)
        {
            if (!InContext.LibraryDocument.IsOpen() || InContext.LibraryDocument.AbsPath() != InPath)
            {
                OPAAX_LOG(LogEditorAnimationLibraryDocument, Warn,
                          "Undo step skipped — its library '{}' is not the open one", InPath.CStr());
                return nullptr;
            }

            return &InContext.LibraryDocument.GetMutableData();
        }
    }

    // =============================================================================
    // LibraryEntriesEdit
    // =============================================================================
    void LibraryEntriesEdit::Undo(EditorContext& InContext)
    {
        if (AnimationLibraryData* lData = TargetLibrary(InContext, LibraryPath)) { lData->Entries = Before; }
    }

    void LibraryEntriesEdit::Redo(EditorContext& InContext)
    {
        if (AnimationLibraryData* lData = TargetLibrary(InContext, LibraryPath)) { lData->Entries = After; }
    }

    // =============================================================================
    // LibraryDefaultClip
    // =============================================================================
    void LibraryDefaultClip::Undo(EditorContext& InContext)
    {
        if (AnimationLibraryData* lData = TargetLibrary(InContext, LibraryPath)) { lData->DefaultClip = Before; }
    }

    void LibraryDefaultClip::Redo(EditorContext& InContext)
    {
        if (AnimationLibraryData* lData = TargetLibrary(InContext, LibraryPath)) { lData->DefaultClip = After; }
    }
}
