#include "Core/String/OpaaxUtf8.h"

#ifdef OPAAX_PLATFORM_WINDOWS

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

namespace Opaax::Utf8
{
    std::wstring ToWide(const OpaaxString& InUtf8)
    {
        if (InUtf8.IsEmpty())
        {
            return {};
        }

        const int lUtf8Len = static_cast<int>(InUtf8.GetLength());
        const int lWideLen = MultiByteToWideChar(CP_UTF8, 0, InUtf8.CStr(), lUtf8Len, nullptr, 0);
        if (lWideLen <= 0)
        {
            return {};
        }

        std::wstring lWide(static_cast<size_t>(lWideLen), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, InUtf8.CStr(), lUtf8Len, lWide.data(), lWideLen);
        return lWide;
    }

    OpaaxString FromWide(const std::wstring& InWide)
    {
        if (InWide.empty())
        {
            return {};
        }

        const int lWideLen = static_cast<int>(InWide.size());
        const int lUtf8Len = WideCharToMultiByte(CP_UTF8, 0, InWide.data(), lWideLen,
                                                 nullptr, 0, nullptr, nullptr);
        if (lUtf8Len <= 0)
        {
            return {};
        }

        std::string lUtf8(static_cast<size_t>(lUtf8Len), '\0');
        WideCharToMultiByte(CP_UTF8, 0, InWide.data(), lWideLen,
                            lUtf8.data(), lUtf8Len, nullptr, nullptr);
        return OpaaxString(lUtf8.c_str());
    }
}

#endif // OPAAX_PLATFORM_WINDOWS
