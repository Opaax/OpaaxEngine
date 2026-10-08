#pragma once

#include <string>

#include "Core/OpaaxTypes.h"

namespace Opaax::Editor
{
    /** One step of an export: what it does, and the command line that does it (run from the workspace root). */
    struct ExportStep
    {
        std::string Description;
        std::string Command;
    };

    // =============================================================================
    // GameExportPlan — the commands that turn a game of the workspace into a folder that runs on
    //   another machine: configure the release preset (a ship build: no editor, the content next to
    //   the executable), build the game, install it into the folder (see CMake/OpaaxGame.cmake).
    //   The commands are built from text the user typed, so what goes in them is checked first.
    // =============================================================================
    namespace GameExportPlan
    {
        /** A game is a CMake target named like its project: letters, digits and '_', not starting with a digit. */
        inline bool IsValidGameName(const std::string& InName)
        {
            if (InName.empty() || (InName[0] >= '0' && InName[0] <= '9'))
            {
                return false;
            }
            for (const char lChar : InName)
            {
                const bool bLetter = (lChar >= 'a' && lChar <= 'z') || (lChar >= 'A' && lChar <= 'Z');
                const bool bDigit  = lChar >= '0' && lChar <= '9';
                if (!bLetter && !bDigit && lChar != '_')
                {
                    return false;
                }
            }
            return true;
        }

        /** A path goes into a command between double quotes: it cannot hold one, nor a line break. */
        inline bool IsSafePath(const std::string& InPath)
        {
            return !InPath.empty() && InPath.find_first_of("\"\r\n") == std::string::npos;
        }

        /** The steps exporting InGame into InDestination, or none when either is not acceptable. */
        inline TDynArray<ExportStep> Make(const std::string& InGame, const std::string& InDestination,
                                          const std::string& InPreset = "release", const std::string& InConfig = "Release")
        {
            if (!IsValidGameName(InGame) || !IsSafePath(InDestination))
            {
                return {};
            }

            const std::string lBuildDir = "build/" + InPreset;
            return {
                { "Configure the " + InPreset + " build", "cmake --preset " + InPreset },
                { "Build " + InGame, "cmake --build " + lBuildDir + " --config " + InConfig + " --target " + InGame },
                { "Install " + InGame + " into " + InDestination,
                  "cmake --install " + lBuildDir + " --config " + InConfig + " --component " + InGame
                      + " --prefix \"" + InDestination + "\"" },
            };
        }

        /**
         * InCommand as one shell line run from InWorkspace, its output added to InLog. On Windows the
         * whole line is quoted once more: cmd.exe /c strips the outer pair.
         */
        inline std::string ShellLine(const std::string& InCommand, const std::string& InWorkspace, const std::string& InLog,
                                     const bool bInWindows)
        {
            const std::string lLine = (bInWindows ? "cd /d \"" : "cd \"") + InWorkspace + "\" && " + InCommand
                                    + " >> \"" + InLog + "\" 2>&1";
            return bInWindows ? "\"" + lLine + "\"" : lLine;
        }
    }
}
