#include "Engine/Subsystems/Resources/Types/AnimationClipFile.h"

#include <nlohmann/json.hpp>

#include "Core/IO/FileIO.h"
#include "Engine/Subsystems/Resources/Types/AnimationClipData.h"

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

        // The SUCCESS branch, not just the failures ([[L15]]).
        OPAAX_LOG(LogAnimationClipFile, Info, "Saved {} step(s) @ {} fps to '{}'",
                  InData.StepCount(), InData.Fps, InAbsPath.CStr());
        return true;
    }

    bool AnimationClipFile::Load(const OpaaxString& InAbsPath, AnimationClipData& OutData)
    {
        const OpaaxString lText = FileIO::ReadAllText(InAbsPath);

        // FileIO answers "" for a missing file and for an empty one alike; neither is a clip.
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

        // _WITH_DEFAULT keeps a default for every absent key, so an older file simply lacks the
        // fields it predates. A WRONG-TYPED value still throws — and so does an unknown PlayMode
        // label, which OpaaxEnumJson refuses on purpose. OutData is untouched on the way out.
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

        OPAAX_LOG(LogAnimationClipFile, Info, "Loaded {} step(s) @ {} fps from '{}'",
                  OutData.StepCount(), OutData.Fps, InAbsPath.CStr());
        return true;
    }
}
