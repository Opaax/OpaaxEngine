#pragma once

#include <string>

namespace Opaax::Editor
{
    struct EditorContext;

    // =============================================================================
    // ExportOps — exporting the edited game (GameExport, in the background), shared by the File
    //   menu and automation. The export builds from the files on disk: what is not saved is not in it.
    // =============================================================================
    namespace ExportOps
    {
        /** The edited game: its project file's name, which is also its CMake target. */
        std::string GameName(const EditorContext& InContext);

        /** Where the export's output goes: <project>/Save/Export.log. */
        std::string LogPath(const EditorContext& InContext);

        /** Starts exporting the edited game into InDestination. False (logged) when refused. */
        bool Start(EditorContext& InContext, const std::string& InDestination);
    }
}
