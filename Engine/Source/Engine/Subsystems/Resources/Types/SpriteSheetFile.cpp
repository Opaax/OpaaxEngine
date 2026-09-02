#include "Engine/Subsystems/Resources/Types/SpriteSheetFile.h"

#include <nlohmann/json.hpp>

#include "Core/IO/FileIO.h"
#include "Engine/Subsystems/Resources/Types/SpriteSheetData.h"

namespace Opaax
{
    OpaaxString SpriteSheetFile::Serialize(const SpriteSheetData& InData)
    {
        const nlohmann::json lJson = InData;

        // dump(4) — the same indent maps and levels use, and nlohmann's object is sorted, so a
        // hand-authored file that reads correctly also WRITES back byte-identical.
        return OpaaxString(lJson.dump(4).c_str());
    }

    bool SpriteSheetFile::Save(const OpaaxString& InAbsPath, const SpriteSheetData& InData)
    {
        if (!FileIO::WriteAllText(InAbsPath, Serialize(InData)))
        {
            OPAAX_LOG(LogSpriteSheetFile, Error, "Cannot write sheet '{}'", InAbsPath.CStr());
            return false;
        }

        // The SUCCESS branch, not just the failures ([[L15]]).
        OPAAX_LOG(LogSpriteSheetFile, Info, "Saved {} frame(s) to '{}'",
                  InData.FrameCount(), InAbsPath.CStr());
        return true;
    }

    bool SpriteSheetFile::Load(const OpaaxString& InAbsPath, SpriteSheetData& OutData)
    {
        const OpaaxString lText = FileIO::ReadAllText(InAbsPath);

        // FileIO answers "" for a missing file and for an empty one alike; neither is a sheet.
        if (lText.IsEmpty())
        {
            OPAAX_LOG(LogSpriteSheetFile, Error, "Sheet '{}' is missing, empty or unreadable", InAbsPath.CStr());
            return false;
        }

        // parse(input, callback, allow_exceptions): no callback, NO EXCEPTIONS — a malformed file is
        // a return value here, exactly as MapJson reads one.
        const nlohmann::json lJson = nlohmann::json::parse(lText.CStr(), nullptr, false);

        if (lJson.is_discarded() || !lJson.is_object())
        {
            OPAAX_LOG(LogSpriteSheetFile, Error, "Sheet '{}' is not a json object", InAbsPath.CStr());
            return false;
        }

        // _WITH_DEFAULT keeps a default for every absent key, so an older file simply lacks the
        // fields it predates. A WRONG-TYPED value still throws, which is what this catch is for —
        // OutData is untouched on the way out.
        try
        {
            OutData = lJson.get<SpriteSheetData>();
        }
        catch (const nlohmann::json::exception& InError)
        {
            OPAAX_LOG(LogSpriteSheetFile, Error, "Sheet '{}' has an unreadable value: {}",
                      InAbsPath.CStr(), InError.what());
            return false;
        }

        OPAAX_LOG(LogSpriteSheetFile, Info, "Loaded {} frame(s) from '{}'",
                  OutData.FrameCount(), InAbsPath.CStr());
        return true;
    }
}
