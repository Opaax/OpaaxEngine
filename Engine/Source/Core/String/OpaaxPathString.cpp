#include "Core/String/OpaaxPathString.h"

#include "Core/String/OpaaxUtf8.h"

#include <filesystem>

namespace Opaax::PathString
{
    namespace
    {
        std::filesystem::path ToPath(const OpaaxStringView InPath)
        {
            return Utf8::ToFsPath(InPath.ToString());
        }
    }

    OpaaxString Join(const OpaaxStringView InA, const OpaaxStringView InB)
    {
        return Utf8::FromFsPath(ToPath(InA) / ToPath(InB));
    }

    OpaaxString Parent(const OpaaxStringView InPath)
    {
        return Utf8::FromFsPath(ToPath(InPath).parent_path());
    }

    OpaaxString Generic(const OpaaxStringView InPath)
    {
        return Utf8::FromFsPath(ToPath(InPath));
    }

    OpaaxString Normalize(const OpaaxStringView InPath)
    {
        return Utf8::FromFsPath(ToPath(InPath).lexically_normal());
    }

    bool IsAbsolute(const OpaaxStringView InPath)
    {
        return ToPath(InPath).is_absolute();
    }

    OpaaxString RelativeTo(const OpaaxStringView InPath, const OpaaxStringView InBase)
    {
        return Utf8::FromFsPath(ToPath(InPath).lexically_relative(ToPath(InBase)));
    }

    OpaaxString ReplaceExtension(const OpaaxStringView InPath, const OpaaxStringView InExtension)
    {
        return Utf8::FromFsPath(ToPath(InPath).replace_extension(ToPath(InExtension)));
    }
}
