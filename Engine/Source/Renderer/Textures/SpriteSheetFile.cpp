#include "Renderer/Textures/SpriteSheetFile.h"

#include <nlohmann/json.hpp>

#include "Core/IO/FileIO.h"
#include "Renderer/Textures/SpriteSheetData.h"

namespace Opaax
{
    OpaaxString SpriteSheetFile::Serialize(const SpriteSheetData& InData)
    {
        const nlohmann::json lJson = InData;

        // dump(4), sorted keys.
        return OpaaxString(lJson.dump(4).c_str());
    }

    bool SpriteSheetFile::Save(const OpaaxString& InAbsPath, const SpriteSheetData& InData)
    {
        if (!FileIO::WriteAllText(InAbsPath, Serialize(InData)))
        {
            OPAAX_LOG(LogSpriteSheetFile, Error, "Cannot write sheet '{}'", InAbsPath.CStr());
            return false;
        }

        // Log success too.
        OPAAX_LOG(LogSpriteSheetFile, Info, "Saved {} frame(s) to '{}'",
                  InData.FrameCount(), InAbsPath.CStr());
        return true;
    }

    bool SpriteSheetFile::Load(const OpaaxString& InAbsPath, SpriteSheetData& OutData)
    {
        const OpaaxString lText = FileIO::ReadAllText(InAbsPath);

        // Missing or empty file: not a sheet.
        if (lText.IsEmpty())
        {
            OPAAX_LOG(LogSpriteSheetFile, Error, "Sheet '{}' is missing, empty or unreadable", InAbsPath.CStr());
            return false;
        }

        // No exceptions: a malformed file is a return value.
        const nlohmann::json lJson = nlohmann::json::parse(lText.CStr(), nullptr, false);

        if (lJson.is_discarded() || !lJson.is_object())
        {
            OPAAX_LOG(LogSpriteSheetFile, Error, "Sheet '{}' is not a json object", InAbsPath.CStr());
            return false;
        }

        // Missing keys keep their defaults. Wrong-typed values throw; OutData is untouched then.
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

        OPAAX_LOG(LogSpriteSheetFile, Trace, "Loaded {} frame(s) from '{}'",
                  OutData.FrameCount(), InAbsPath.CStr());
        return true;
    }
}
