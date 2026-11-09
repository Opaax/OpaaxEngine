#include "Online/Steam/SteamLibrary.h"

#include "Core/EngineAPI.h"

namespace Opaax
{
    const char* SteamLibrary::GetFileName() noexcept
    {
#if defined(OPAAX_PLATFORM_WINDOWS)
        return "steam_api64.dll";
#elif defined(OPAAX_PLATFORM_MACOS)
        return "libsteam_api.dylib";
#else
        return "libsteam_api.so";
#endif
    }

    bool SteamLibrary::Load(const OpaaxString& InPath, OpaaxString& OutError)
    {
        if (!Library.Open(InPath))
        {
            OutError = OpaaxString::Format("{} not loaded: {}", GetFileName(), Library.GetError().CStr());
            return false;
        }

#define OPAAX_STEAM_FIND_FUNCTION(Name)                                                                    \
        Name = Library.FindFunction<decltype(&::Name)>(#Name);                                            \
        if (Name == nullptr)                                                                               \
        {                                                                                                  \
            OutError = OpaaxString::Format("{} has no {}: it is older than the SDK the game was built with", \
                                           GetFileName(), #Name);                                          \
            Library.Close();                                                                               \
            return false;                                                                                  \
        }

        OPAAX_STEAM_FUNCTIONS(OPAAX_STEAM_FIND_FUNCTION)
#undef OPAAX_STEAM_FIND_FUNCTION

        return true;
    }
}
