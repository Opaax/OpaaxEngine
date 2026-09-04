#include "Editor/Undo/FontFamilyUndoables.h"

#include "Editor/EditorContext.h"
#include "Editor/EditorFontFamilyDocument.h"

namespace Opaax::Editor
{
    namespace
    {
        /** The open family's data when it is the one InPath names, else null WITH A LINE SAYING SO. */
        FontFamilyData* TargetFamily(EditorContext& InContext, const OpaaxString& InPath)
        {
            if (!InContext.FamilyDocument.IsOpen() || InContext.FamilyDocument.AbsPath() != InPath)
            {
                OPAAX_LOG(LogEditorFontFamilyDocument, Warn,
                          "Undo step skipped — its family '{}' is not the open one", InPath.CStr());
                return nullptr;
            }

            return &InContext.FamilyDocument.GetMutableData();
        }
    }

    void FontFamilyEntriesEdit::Undo(EditorContext& InContext)
    {
        if (FontFamilyData* lData = TargetFamily(InContext, FamilyPath)) { lData->Entries = Before; }
    }

    void FontFamilyEntriesEdit::Redo(EditorContext& InContext)
    {
        if (FontFamilyData* lData = TargetFamily(InContext, FamilyPath)) { lData->Entries = After; }
    }
}
