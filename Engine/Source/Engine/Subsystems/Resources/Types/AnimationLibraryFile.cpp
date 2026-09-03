#include "Engine/Subsystems/Resources/Types/AnimationLibraryFile.h"

#include <nlohmann/json.hpp>

#include "Core/IO/FileIO.h"
#include "Engine/Subsystems/Resources/Types/AnimationLibraryData.h"

namespace Opaax
{
    OpaaxString AnimationLibraryFile::Serialize(const AnimationLibraryData& InData)
    {
        const nlohmann::json lJson = InData;

        return OpaaxString(lJson.dump(4).c_str());
    }

    bool AnimationLibraryFile::Save(const OpaaxString& InAbsPath, const AnimationLibraryData& InData)
    {
        if (!FileIO::WriteAllText(InAbsPath, Serialize(InData)))
        {
            OPAAX_LOG(LogAnimationLibraryFile, Error, "Cannot write library '{}'", InAbsPath.CStr());
            return false;
        }

        OPAAX_LOG(LogAnimationLibraryFile, Info, "Saved {} clip name(s) to '{}'",
                  InData.EntryCount(), InAbsPath.CStr());
        return true;
    }

    bool AnimationLibraryFile::Load(const OpaaxString& InAbsPath, AnimationLibraryData& OutData)
    {
        const OpaaxString lText = FileIO::ReadAllText(InAbsPath);

        if (lText.IsEmpty())
        {
            OPAAX_LOG(LogAnimationLibraryFile, Error, "Library '{}' is missing, empty or unreadable",
                      InAbsPath.CStr());
            return false;
        }

        const nlohmann::json lJson = nlohmann::json::parse(lText.CStr(), nullptr, false);

        if (lJson.is_discarded() || !lJson.is_object())
        {
            OPAAX_LOG(LogAnimationLibraryFile, Error, "Library '{}' is not a json object", InAbsPath.CStr());
            return false;
        }

        try
        {
            OutData = lJson.get<AnimationLibraryData>();
        }
        catch (const nlohmann::json::exception& InError)
        {
            OPAAX_LOG(LogAnimationLibraryFile, Error, "Library '{}' has an unreadable value: {}",
                      InAbsPath.CStr(), InError.what());
            return false;
        }

        OPAAX_LOG(LogAnimationLibraryFile, Info, "Loaded {} clip name(s) from '{}'",
                  OutData.EntryCount(), InAbsPath.CStr());
        return true;
    }
}
