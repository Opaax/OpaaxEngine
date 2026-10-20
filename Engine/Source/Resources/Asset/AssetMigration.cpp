#include "Resources/Asset/AssetMigration.h"

#include <nlohmann/json.hpp>

#include "Core/Algo/Algo.h"
#include "Core/IO/FileIO.h"
#include "Core/IO/FileSystem.h"
#include "Core/String/Char.h"
#include "Core/String/OpaaxPathString.h"
#include "Core/String/OpaaxStringJson.h"
#include "Resources/Asset/AssetFile.h"

namespace Opaax
{
    namespace
    {
        /**
         * The files whose text may name an asset: every JSON file a project keeps, the old per-type asset
         * formats included (one not converted yet can name one that was).
         */
        constexpr const char* k_JsonExtensions[] = {
            ".opaaxlevel", ".opaaxmap", ".opaaxprefab", ".opaaxui", ".opaaxasset", ".opaaxproj", ".config",
            ".opaaxdata", ".opaaxmovemode", ".opaaxmover", ".opaaxaction", ".opaaxinputmap", ".opaaxclip",
            ".opaaxanim", ".opaaxsheet", ".opaaxfont"
        };

        bool EqualsIgnoreCase(const OpaaxStringView InA, const OpaaxStringView InB)
        {
            if (InA.GetLength() != InB.GetLength())
            {
                return false;
            }

            for (Uint32 lIndex = 0; lIndex < InA.GetLength(); ++lIndex)
            {
                if (Char::ToLower(InA[lIndex]) != Char::ToLower(InB[lIndex])) { return false; }
            }

            return true;
        }

        bool IsJsonFile(const OpaaxStringView InPath)
        {
            const OpaaxStringView lExtension = PathString::Extension(InPath);

            for (const char* lJson : k_JsonExtensions)
            {
                if (EqualsIgnoreCase(lExtension, OpaaxStringView(lJson))) { return true; }
            }

            return false;
        }

        /** Every file under InDir, at any depth, sorted (the same run gives the same result). */
        void ListFiles(const OpaaxString& InDir, TDynArray<OpaaxString>& OutFiles)
        {
            TDynArray<FileSystem::Entry> lEntries;
            if (!FileSystem::ListDirectory(InDir, lEntries))
            {
                return;
            }

            for (const FileSystem::Entry& lEntry : lEntries)
            {
                if (lEntry.bIsDirectory) { ListFiles(lEntry.AbsPath, OutFiles); }
                else                     { OutFiles.emplace_back(lEntry.AbsPath); }
            }
        }

        TDynArray<OpaaxString> SortedFiles(const OpaaxString& InDir)
        {
            TDynArray<OpaaxString> lFiles;
            ListFiles(InDir, lFiles);
            Algo::Sort(lFiles, [](const OpaaxString& InA, const OpaaxString& InB) { return InA < InB; });
            return lFiles;
        }

        /** InAbsPath as files name it: the root's mount, then the path inside the root. */
        OpaaxString NameOf(const AssetMigration::AssetRoot& InRoot, const OpaaxString& InAbsPath)
        {
            return InRoot.Mount + PathString::RelativeTo(InAbsPath, InRoot.AbsPath);
        }

        /** The file a path names: a mount's root for "/Engine/...", else the root without a mount. */
        OpaaxString Resolve(const TDynArray<AssetMigration::AssetRoot>& InRoots, const OpaaxStringView InPath)
        {
            for (const AssetMigration::AssetRoot& lRoot : InRoots)
            {
                if (!lRoot.Mount.IsEmpty() && InPath.StartsWith(OpaaxStringView(lRoot.Mount)))
                {
                    return PathString::Join(lRoot.AbsPath, InPath.SubString(lRoot.Mount.GetLength()));
                }
            }

            if (InPath.IsEmpty() || InPath[0u] == '/' || PathString::IsAbsolute(InPath))
            {
                return OpaaxString();
            }

            for (const AssetMigration::AssetRoot& lRoot : InRoots)
            {
                if (lRoot.Mount.IsEmpty())
                {
                    return PathString::Join(lRoot.AbsPath, InPath);
                }
            }

            return OpaaxString();
        }

        /** Where an old file goes: its stem with .opaaxasset, else with its class appended, else numbered. */
        OpaaxString PlanNewPath(const OpaaxString& InOldAbsPath, const OpaaxStringID InClass,
                                const TDynArray<OpaaxString>& InTaken)
        {
            const auto lFree = [&InTaken](const OpaaxString& InPath)
            {
                return !FileSystem::Exists(InPath) && Algo::Find(InTaken, InPath) == InTaken.end();
            };

            const OpaaxString lDefault = PathString::ReplaceExtension(InOldAbsPath, AssetFile::ASSET_EXTENSION);
            if (lFree(lDefault))
            {
                return lDefault;
            }

            const OpaaxString lBase = PathString::Join(PathString::Parent(InOldAbsPath),
                                                       PathString::Stem(InOldAbsPath).ToString() + "_" + InClass.ToString());

            OpaaxString lPath = lBase + AssetFile::ASSET_EXTENSION;
            for (Uint32 lNumber = 2; !lFree(lPath); ++lNumber)
            {
                lPath = lBase + OpaaxString::FromUInt(lNumber) + AssetFile::ASSET_EXTENSION;
            }

            return lPath;
        }
    }

    const TDynArray<AssetMigration::LegacyFormat>& AssetMigration::GetLegacyFormats()
    {
        // One line per format, added when its type became an Asset class.
        static const TDynArray<LegacyFormat> s_Formats = {
            { ".opaaxdata",      nullptr },
            { ".opaaxmovemode",  "MoveMode" },
            { ".opaaxmover",     "Mover" },
            { ".opaaxaction",    "InputActionAsset" },
            { ".opaaxinputmap",  "InputMappingContextAsset" },
        };

        return s_Formats;
    }

    const AssetMigration::LegacyFormat* AssetMigration::FindLegacyFormat(const OpaaxStringView InPath)
    {
        const OpaaxStringView lExtension = PathString::Extension(InPath);

        for (const LegacyFormat& lFormat : GetLegacyFormats())
        {
            if (EqualsIgnoreCase(lExtension, OpaaxStringView(lFormat.Extension))) { return &lFormat; }
        }

        return nullptr;
    }

    OpaaxString AssetMigration::ConvertedPath(const OpaaxStringView InPath)
    {
        if (FindLegacyFormat(InPath) == nullptr)
        {
            return OpaaxString();
        }

        return PathString::ReplaceExtension(InPath, AssetFile::ASSET_EXTENSION);
    }

    bool AssetMigration::ConvertText(const OpaaxString& InLegacyText, const LegacyFormat& InFormat,
                                     OpaaxString& OutAssetText, OpaaxString* OutError)
    {
        const auto lFail = [OutError](const char* InWhy)
        {
            if (OutError != nullptr) { *OutError = OpaaxString(InWhy); }
            return false;
        };

        const nlohmann::json lJson = nlohmann::json::parse(InLegacyText.CStr(), nullptr, false);

        if (lJson.is_discarded() || !lJson.is_object())
        {
            return lFail("not a JSON object");
        }

        if (InFormat.ClassName != nullptr)
        {
            // The whole object was the properties; the extension named the class.
            OutAssetText = AssetFile::Serialize(OpaaxStringID(InFormat.ClassName), lJson);
            return true;
        }

        // A data asset named its type and kept its values under "Data".
        const auto lType = lJson.find("Type");
        const auto lData = lJson.find("Data");

        if (lType == lJson.end() || !lType->is_string() || lType->get<OpaaxString>().IsEmpty()
            || lData == lJson.end() || !lData->is_object())
        {
            return lFail("a data asset needs a \"Type\" name and a \"Data\" object");
        }

        OutAssetText = AssetFile::Serialize(OpaaxStringID(lType->get<OpaaxString>()), *lData);
        return true;
    }

    Uint32 AssetMigration::RewritePaths(OpaaxString& InOutText,
                                        const TFunction<OpaaxString(OpaaxStringView InPath)>& InRename)
    {
        const OpaaxStringView lText(InOutText);
        const Uint32          lLength = lText.GetLength();

        OpaaxString lResult;
        Uint32      lCopiedTo = 0;
        Uint32      lChanged  = 0;

        for (Uint32 lIndex = 0; lIndex < lLength; ++lIndex)
        {
            if (lText[lIndex] != '"')
            {
                continue;
            }

            // A string: up to the next quote that is not escaped.
            Uint32 lEnd     = lIndex + 1;
            bool   bEscaped = false;
            while (lEnd < lLength && (lText[lEnd] != '"' || bEscaped))
            {
                bEscaped = !bEscaped && lText[lEnd] == '\\';
                ++lEnd;
            }

            if (lEnd >= lLength)
            {
                break;   // unterminated: leave the rest as it is
            }

            const OpaaxStringView lValue = lText.SubString(lIndex + 1, lEnd - lIndex - 1);

            if (FindLegacyFormat(lValue) != nullptr)
            {
                const OpaaxString lNew = InRename(lValue);
                if (!lNew.IsEmpty() && OpaaxStringView(lNew) != lValue)
                {
                    lResult.Append(lText.SubString(lCopiedTo, lIndex + 1 - lCopiedTo));
                    lResult.Append(lNew);
                    lCopiedTo = lEnd;   // the closing quote goes with what follows
                    ++lChanged;
                }
            }

            lIndex = lEnd;
        }

        if (lChanged > 0)
        {
            lResult.Append(lText.SubString(lCopiedTo));
            InOutText = Move(lResult);
        }

        return lChanged;
    }

    Uint32 AssetMigration::CountLegacyFiles(const TDynArray<AssetRoot>& InRoots)
    {
        Uint32 lCount = 0;

        for (const AssetRoot& lRoot : InRoots)
        {
            TDynArray<OpaaxString> lFiles;
            ListFiles(lRoot.AbsPath, lFiles);

            for (const OpaaxString& lFile : lFiles)
            {
                if (FindLegacyFormat(lFile) != nullptr) { ++lCount; }
            }
        }

        return lCount;
    }

    AssetMigration::Report AssetMigration::Migrate(const TDynArray<AssetRoot>& InRoots,
                                                   const TDynArray<OpaaxString>& InAlsoRewrite)
    {
        Report                 lReport;
        TDynArray<OpaaxString> lTaken;

        const auto lError = [&lReport](OpaaxString InLine)
        {
            OPAAX_LOG(LogAssetMigration, Warn, "{}", InLine.CStr());
            lReport.Errors.emplace_back(Move(InLine));
        };

        // ---- Every old file becomes an asset file of its class ---------------------------------------
        for (const AssetRoot& lRoot : InRoots)
        {
            for (const OpaaxString& lFile : SortedFiles(lRoot.AbsPath))
            {
                const LegacyFormat* lFormat = FindLegacyFormat(lFile);
                if (lFormat == nullptr)
                {
                    continue;
                }

                const OpaaxString lName = NameOf(lRoot, lFile);
                const OpaaxString lText = FileIO::ReadAllText(lFile);

                OpaaxString         lAssetText;
                OpaaxString         lWhy;
                AssetFile::Contents lContents;
                if (lText.IsEmpty() || !ConvertText(lText, *lFormat, lAssetText, &lWhy)
                    || !AssetFile::Parse(lAssetText, lContents, &lWhy))
                {
                    lError(OpaaxString::Format("'{}' was not converted: {}", lName.CStr(),
                                               lText.IsEmpty() ? "it is empty or unreadable" : lWhy.CStr()));
                    continue;
                }

                const OpaaxString lNewFile = PlanNewPath(lFile, lContents.Class, lTaken);

                if (!FileIO::WriteAllText(lNewFile, lAssetText))
                {
                    lError(OpaaxString::Format("'{}' was not converted: cannot write '{}'", lName.CStr(), lNewFile.CStr()));
                    continue;
                }

                if (!FileSystem::Remove(lFile))
                {
                    // Both would load otherwise: the new one goes, the old one stays as it was.
                    FileSystem::Remove(lNewFile);
                    lError(OpaaxString::Format("'{}' was not converted: it cannot be removed", lName.CStr()));
                    continue;
                }

                lTaken.emplace_back(lNewFile);
                lReport.Converted.emplace_back(lName, NameOf(lRoot, lNewFile));

                OPAAX_LOG(LogAssetMigration, Info, "Converted '{}' -> '{}' ({})", lName.CStr(),
                          lReport.Converted.back().second.CStr(), lContents.Class);
            }
        }

        // ---- Every path that names an old file ----------------------------------------------------------
        const auto lRename = [&InRoots, &lReport](const OpaaxStringView InPath) -> OpaaxString
        {
            for (const TPair<OpaaxString, OpaaxString>& lConverted : lReport.Converted)
            {
                if (OpaaxStringView(lConverted.first) == InPath) { return lConverted.second; }
            }

            // Converted by an earlier run (another project sharing the engine's content): the default name.
            const OpaaxString lDefault = PathString::ReplaceExtension(InPath, AssetFile::ASSET_EXTENSION);
            const OpaaxString lTarget  = Resolve(InRoots, OpaaxStringView(lDefault));

            return (!lTarget.IsEmpty() && FileSystem::IsFile(lTarget)) ? lDefault : OpaaxString();
        };

        TDynArray<OpaaxString> lFolders;
        for (const AssetRoot& lRoot : InRoots) { lFolders.emplace_back(lRoot.AbsPath); }
        for (const OpaaxString& lFolder : InAlsoRewrite) { lFolders.emplace_back(lFolder); }

        for (const OpaaxString& lFolder : lFolders)
        {
            for (const OpaaxString& lFile : SortedFiles(lFolder))
            {
                if (!IsJsonFile(lFile))
                {
                    continue;
                }

                OpaaxString  lText    = FileIO::ReadAllText(lFile);
                const Uint32 lChanged = RewritePaths(lText, lRename);

                if (lChanged == 0)
                {
                    continue;
                }

                if (!FileIO::WriteAllText(lFile, lText))
                {
                    lError(OpaaxString::Format("'{}' names old files but cannot be written", lFile.CStr()));
                    continue;
                }

                lReport.Rewritten.emplace_back(lFile);
                OPAAX_LOG(LogAssetMigration, Info, "Rewrote {} path(s) in '{}'", lChanged, lFile.CStr());
            }
        }

        return lReport;
    }
}
