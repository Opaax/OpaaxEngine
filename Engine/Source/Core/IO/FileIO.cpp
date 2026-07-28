#include "Core/IO/FileIO.h"

#include "Core/String/OpaaxUtf8.h"   // I7 — never open a stream from CStr()

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

        // Fill a local first: OutBytes must be untouched if the read fails half-way.
        TDynArray<Uint8> lBytes(static_cast<size_t>(lSize));

        lFile.seekg(0, std::ios::beg);
        if (lSize > 0 && !lFile.read(reinterpret_cast<char*>(lBytes.data()), lSize))
        {
            return false;
        }

        OutBytes = Move(lBytes);
        return true;
    }

    bool WriteAllText(const OpaaxString& InAbsPath, const OpaaxString& InText)
    {
        if (InAbsPath.IsEmpty())
        {
            return false;
        }

        const fs::path lPath = Utf8::ToFsPath(InAbsPath);

        if (lPath.has_parent_path())
        {
            // error_code overload: a missing parent is an ordinary failure, not an exception. It
            // reports false when the directory already existed, which is a success here — so ask
            // what is true now rather than trusting the return (same contract as IFileSystem).
            std::error_code lError;
            fs::create_directories(lPath.parent_path(), lError);

            if (!fs::is_directory(lPath.parent_path(), lError) || lError)
            {
                return false;
            }
        }

        std::ofstream lFile(lPath);
        if (!lFile.is_open())
        {
            return false;
        }

        lFile << InText.CStr();
        return lFile.good();
    }
}
