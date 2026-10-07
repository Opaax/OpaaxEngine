#include "Core/IO/FileIO.h"

#include "Core/String/OpaaxUtf8.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <system_error>

namespace Opaax::FileIO
{
    namespace fs = std::filesystem;

    OpaaxString ReadAllText(const OpaaxString& InAbsPath)
    {
        if (InAbsPath.IsEmpty())
        {
            return {};
        }

        std::ifstream lFile(Utf8::ToFsPath(InAbsPath));
        if (!lFile.is_open())
        {
            return {};
        }

        std::stringstream lBuffer;
        lBuffer << lFile.rdbuf();
        return OpaaxString(lBuffer.str().c_str());
    }

    bool ReadAllBytes(const OpaaxString& InAbsPath, TDynArray<Uint8>& OutBytes)
    {
        if (InAbsPath.IsEmpty())
        {
            return false;
        }

        std::ifstream lFile(Utf8::ToFsPath(InAbsPath), std::ios::binary | std::ios::ate);
        if (!lFile.is_open())
        {
            return false;
        }

        const std::streamsize lSize = lFile.tellg();
        if (lSize < 0)
        {
            return false;
        }

        // Fill a local first: OutBytes stays untouched on failure.
        TDynArray<Uint8> lBytes(static_cast<size_t>(lSize));

        lFile.seekg(0, std::ios::beg);
        if (lSize > 0 && !lFile.read(reinterpret_cast<char*>(lBytes.data()), lSize))
        {
            return false;
        }

        OutBytes = Move(lBytes);
        return true;
    }

    namespace
    {
        /** The parent folders of InPath, created if missing. */
        bool EnsureParentDirectory(const fs::path& InPath)
        {
            if (!InPath.has_parent_path())
            {
                return true;
            }

            // Returns false if the directory already existed, so check that it exists now.
            std::error_code lError;
            fs::create_directories(InPath.parent_path(), lError);
            return fs::is_directory(InPath.parent_path(), lError) && !lError;
        }
    }

    bool WriteAllText(const OpaaxString& InAbsPath, const OpaaxString& InText)
    {
        if (InAbsPath.IsEmpty())
        {
            return false;
        }

        const fs::path lPath = Utf8::ToFsPath(InAbsPath);
        if (!EnsureParentDirectory(lPath))
        {
            return false;
        }

        std::ofstream lFile(lPath);
        if (!lFile.is_open())
        {
            return false;
        }

        lFile << InText.CStr();
        return lFile.good();
    }

    bool WriteAllBytes(const OpaaxString& InAbsPath, const Uint8* InData, const Uint64 InSize)
    {
        if (InAbsPath.IsEmpty() || (InData == nullptr && InSize > 0))
        {
            return false;
        }

        const fs::path lPath = Utf8::ToFsPath(InAbsPath);
        if (!EnsureParentDirectory(lPath))
        {
            return false;
        }

        std::ofstream lFile(lPath, std::ios::binary | std::ios::trunc);
        if (!lFile.is_open())
        {
            return false;
        }

        lFile.write(reinterpret_cast<const char*>(InData), static_cast<std::streamsize>(InSize));
        return lFile.good();
    }
}
