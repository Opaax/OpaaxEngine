#pragma once

#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"

namespace Opaax::Editor
{
    /** One step of an export: what it does, and the command line that does it (run from the workspace root). */
    struct ExportStep
    {
        OpaaxString Description;
        OpaaxString Command;
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
        inline bool IsValidGameName(const OpaaxStringView InName)
        {
            if (InName.IsEmpty() || (InName[0] >= '0' && InName[0] <= '9'))
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
        inline bool IsSafePath(const OpaaxStringView InPath)
        {
            return !InPath.IsEmpty() && InPath.FindFirstOf("\"\r\n") < 0;
        }

        /** The steps exporting InGame into InDestination, or none when either is not acceptable. */
        inline TDynArray<ExportStep> Make(const OpaaxString& InGame, const OpaaxString& InDestination,
                                          const OpaaxString& InPreset = "release", const OpaaxString& InConfig = "Release")
        {
            if (!IsValidGameName(InGame) || !IsSafePath(InDestination))
            {
                return {};
            }

            const OpaaxString lBuildDir = "build/" + InPreset;
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
        inline OpaaxString ShellLine(const OpaaxString& InCommand, const OpaaxString& InWorkspace, const OpaaxString& InLog,
                                     const bool bInWindows)
        {
            const OpaaxString lLine = (bInWindows ? "cd /d \"" : "cd \"") + InWorkspace + "\" && " + InCommand
                                    + " >> \"" + InLog + "\" 2>&1";
            return bInWindows ? "\"" + lLine + "\"" : lLine;
        }
    }
}
