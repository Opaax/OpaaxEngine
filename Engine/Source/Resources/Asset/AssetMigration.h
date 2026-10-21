#pragma once

#include "Core/EngineAPI.h"
#include "Core/Log/Logger.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"
#include "Core/String/OpaaxStringView.hpp"
#include "Resources/Asset/AssetFile.h"

namespace Opaax
{
    OPAAX_LOG_CATEGORY(AssetMigration);

    // =============================================================================
    // AssetMigration — brings a project's files from the per-type asset formats the engine used before
    //   (.opaaxdata, .opaaxmovemode...) to .opaaxasset: each old file becomes an asset file of its class,
    //   and every path that names it, in levels, maps, prefabs, canvases, assets and configs, is
    //   rewritten. Code that names a file in a string is not touched: search it for the old extensions.
    //   An asset file of a class since split into subclasses (GetClassSplits) is rewritten in its subclass.
    // =============================================================================
    namespace AssetMigration
    {
        /** A format used before .opaaxasset. */
        struct LegacyFormat
        {
            /** ".opaaxmovemode" */
            const char* Extension = nullptr;

            /** The class its files hold. Null for .opaaxdata, whose files name their type. */
            const char* ClassName = nullptr;
        };

        /** Every old format this engine converts. */
        const TDynArray<LegacyFormat>& GetLegacyFormats();

        /**
         * A class split into subclasses: each of its files names the subclass it holds in a property
         * (a MoveMode's "Mode" became the class GroundMoveMode, FlyMoveMode...).
         */
        struct ClassSplit
        {
            /** The class files named before the split: "MoveMode". */
            const char* Class = nullptr;

            /** The property naming a file's subclass, which the split removes: "Mode". */
            const char* ByProperty = nullptr;

            /** Appended to that property's value to name the subclass: "GroundMove" -> "GroundMoveMode". */
            const char* Suffix = nullptr;
        };

        /** Every class split this engine upgrades. */
        const TDynArray<ClassSplit>& GetClassSplits();

        /**
         * Gives InOutContents the subclass it holds when its class was split, and removes the property
         * that named it. Its other properties are kept: those the subclass lacks are ignored on load.
         * @return False when its class was not split or the property is missing (nothing changed)
         */
        bool UpgradeSplitClass(AssetFile::Contents& InOutContents);

        /** The old format of InPath's extension (any case), or null. */
        const LegacyFormat* FindLegacyFormat(OpaaxStringView InPath);

        /**
         * The path an old file has once converted, when it kept its name ("Input/Jump.opaaxaction" ->
         * "Input/Jump.opaaxasset"); empty when InPath has no old extension. For paths written in code,
         * which Migrate does not rewrite.
         */
        OpaaxString ConvertedPath(OpaaxStringView InPath);

        /**
         * An old file's text as an .opaaxasset's: the class and the properties it holds.
         * @param OutError Why it failed (optional)
         * @return False when the text is not a JSON object, or a .opaaxdata lacks its "Type" or "Data"
         */
        bool ConvertText(const OpaaxString& InLegacyText, const LegacyFormat& InFormat, OpaaxString& OutAssetText,
                         OpaaxString* OutError = nullptr);

        /**
         * InText with every quoted path that InRename maps changed to what it returns (empty: unchanged).
         * Only strings ending in an old format's extension are offered to InRename.
         * @return How many paths changed
         */
        Uint32 RewritePaths(OpaaxString& InOutText, const TFunction<OpaaxString(OpaaxStringView InPath)>& InRename);

        /** A folder of assets, and the prefix files name it by: "" for a project's, "/Engine/" for the engine's. */
        struct AssetRoot
        {
            OpaaxString AbsPath;
            OpaaxString Mount;
        };

        /** How many old asset files InRoots hold (what Migrate would convert). */
        Uint32 CountLegacyFiles(const TDynArray<AssetRoot>& InRoots);

        /** What a migration did. */
        struct Report
        {
            /** Each converted file, as files name it: old path, new path. */
            TDynArray<TPair<OpaaxString, OpaaxString>> Converted;

            /** Every file whose paths were rewritten (absolute). */
            TDynArray<OpaaxString> Rewritten;

            /** Every asset file rewritten in the subclass its split class became (absolute). */
            TDynArray<OpaaxString> Upgraded;

            /** What could not be done, one line each. A file that failed keeps its old form. */
            TDynArray<OpaaxString> Errors;
        };

        /**
         * Converts every old asset file under InRoots to .opaaxasset (the old file is deleted once the new
         * one is written; a taken name gets the class appended), rewrites in its subclass every asset file
         * of a split class, then rewrites the paths that name an old file in every JSON file of InRoots
         * and InAlsoRewrite (a project's Configs). Paths to files converted by an earlier run (another
         * project sharing the engine's content) are rewritten too. Running it again finds nothing to do.
         */
        Report Migrate(const TDynArray<AssetRoot>& InRoots, const TDynArray<OpaaxString>& InAlsoRewrite = {});
    }
}
