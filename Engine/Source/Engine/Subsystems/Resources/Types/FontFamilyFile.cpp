#include "Engine/Subsystems/Resources/Types/FontFamilyFile.h"

#include <nlohmann/json.hpp>

#include "Core/IO/FileIO.h"
#include "Engine/Subsystems/Resources/Types/FontFamilyData.h"

namespace Opaax
{
    OpaaxString FontFamilyFile::Serialize(const FontFamilyData& InData)
    {
        const nlohmann::json lJson = InData;

        return OpaaxString(lJson.dump(4).c_str());
    }

    bool FontFamilyFile::Save(const OpaaxString& InAbsPath, const FontFamilyData& InData)
    {
        if (!FileIO::WriteAllText(InAbsPath, Serialize(InData)))
        {
            OPAAX_LOG(LogFontFamilyFile, Error, "Cannot write family '{}'", InAbsPath.CStr());
            return false;
        }

        OPAAX_LOG(LogFontFamilyFile, Info, "Saved {} face(s) to '{}'",
                  InData.EntryCount(), InAbsPath.CStr());
        return true;
    }

    bool FontFamilyFile::Load(const OpaaxString& InAbsPath, FontFamilyData& OutData)
    {
        const OpaaxString lText = FileIO::ReadAllText(InAbsPath);

        if (lText.IsEmpty())
        {
            OPAAX_LOG(LogFontFamilyFile, Error, "Family '{}' is missing, empty or unreadable", InAbsPath.CStr());
            return false;
        }

        const nlohmann::json lJson = nlohmann::json::parse(lText.CStr(), nullptr, false);

        if (lJson.is_discarded() || !lJson.is_object())
        {
            OPAAX_LOG(LogFontFamilyFile, Error, "Family '{}' is not a json object", InAbsPath.CStr());
            return false;
        }

        // A misspelled enumerator THROWS out of the enum bridge rather than silently reading as the
        // first value — which is what this catch is for, and why OutData is untouched on the way out.
        try
        {
            OutData = lJson.get<FontFamilyData>();
        }
        catch (const nlohmann::json::exception& InError)
        {
            OPAAX_LOG(LogFontFamilyFile, Error, "Family '{}' has an unreadable value: {}",
                      InAbsPath.CStr(), InError.what());
            return false;
        }

        OPAAX_LOG(LogFontFamilyFile, Info, "Loaded {} face(s) from '{}'",
                  OutData.EntryCount(), InAbsPath.CStr());
        return true;
    }
}
