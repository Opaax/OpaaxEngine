#include "Editor/Operation/ExportOperations.h"

#include "Application/Services/IPaths.h"
#include "Core/String/OpaaxPathString.h"
#include "Editor/EditorContext.h"
#include "Editor/Export/GameExport.h"

namespace Opaax::Editor::ExportOps
{
    OpaaxString GameName(const EditorContext& InContext)
    {
        return PathString::Stem(InContext.Paths.ProjectFile()).ToString();
    }

    OpaaxString LogPath(const EditorContext& InContext)
    {
        return PathString::Join(InContext.Paths.SaveDir(), "Export.log");
    }

    bool Start(EditorContext& InContext, const OpaaxString& InDestination)
    {
        OPAAX_LOG(LogGameExport, Info, "The export builds from the saved files: unsaved changes are not in it");
        return InContext.Export.Start(InContext.Paths.WorkspaceRoot(), GameName(InContext), InDestination,
                                      LogPath(InContext));
    }
}
