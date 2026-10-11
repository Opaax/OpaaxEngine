#include "Editor/Operation/ExportOperations.h"

#include <filesystem>

#include "Application/Services/IPaths.h"
#include "Editor/EditorContext.h"
#include "Editor/Export/GameExport.h"

namespace Opaax::Editor::ExportOps
{
    std::string GameName(const EditorContext& InContext)
    {
        return std::filesystem::path(InContext.Paths.ProjectFile().CStr()).stem().string();
    }

    std::string LogPath(const EditorContext& InContext)
    {
        return (std::filesystem::path(InContext.Paths.SaveDir().CStr()) / "Export.log").generic_string();
    }

    bool Start(EditorContext& InContext, const std::string& InDestination)
    {
        OPAAX_LOG(LogGameExport, Info, "The export builds from the saved files: unsaved changes are not in it");
        return InContext.Export.Start(std::string(InContext.Paths.WorkspaceRoot().CStr()), GameName(InContext),
                                      InDestination, LogPath(InContext));
    }
}
