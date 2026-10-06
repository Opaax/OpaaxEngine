#include "Engine/Subsystems/Resources/Types/Animation/AnimationClipFile.h"

#include <nlohmann/json.hpp>

#include "Core/IO/FileIO.h"
#include "Engine/Subsystems/Resources/Types/Animation/AnimationClipData.h"

namespace Opaax
{
    OpaaxString AnimationClipFile::Serialize(const AnimationClipData& InData)
    {
        const nlohmann::json lJson = InData;

        return OpaaxString(lJson.dump(4).c_str());
    }

    bool AnimationClipFile::Save(const OpaaxString& InAbsPath, const AnimationClipData& InData)
    {
        if (!FileIO::WriteAllText(InAbsPath, Serialize(InData)))
        {
            OPAAX_LOG(LogAnimationClipFile, Error, "Cannot write clip '{}'", InAbsPath.CStr());
            return false;
        }

        // Log success too.
        OPAAX_LOG(LogAnimationClipFile, Info, "Saved {} step(s) @ {} fps to '{}'",
                  InData.StepCount(), InData.Fps, InAbsPath.CStr());
        return true;
    }

    bool AnimationClipFile::Load(const OpaaxString& InAbsPath, AnimationClipData& OutData)
    {
        const OpaaxString lText = FileIO::ReadAllText(InAbsPath);

        // Missing or empty file: not a clip.
        if (lText.IsEmpty())
        {
            OPAAX_LOG(LogAnimationClipFile, Error, "Clip '{}' is missing, empty or unreadable", InAbsPath.CStr());
            return false;
        }

        const nlohmann::json lJson = nlohmann::json::parse(lText.CStr(), nullptr, false);

        if (lJson.is_discarded() || !lJson.is_object())
        {
            OPAAX_LOG(LogAnimationClipFile, Error, "Clip '{}' is not a json object", InAbsPath.CStr());
            return false;
        }

        // Missing keys keep their defaults. Wrong-typed values (or an unknown PlayMode) throw;
        // OutData is untouched then.
        try
        {
            OutData = lJson.get<AnimationClipData>();
        }
        catch (const nlohmann::json::exception& InError)
        {
            OPAAX_LOG(LogAnimationClipFile, Error, "Clip '{}' has an unreadable value: {}",
                      InAbsPath.CStr(), InError.what());
            return false;
        }

        OPAAX_LOG(LogAnimationClipFile, Trace, "Loaded {} step(s) @ {} fps from '{}'",
                  OutData.StepCount(), OutData.Fps, InAbsPath.CStr());
        return true;
    }
}
