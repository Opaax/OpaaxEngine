#include "ConfigIO.h"

#include <filesystem>
#include <fstream>
#include <sstream>

namespace Opaax::ConfigIO
{
    namespace fs = std::filesystem;

    OpaaxString ReadText(const OpaaxString& InAbsPath)
    {
        std::ifstream lFile(InAbsPath.CStr());
        if (!lFile.is_open()) { return OpaaxString(); }

        std::stringstream lBuffer;
        lBuffer << lFile.rdbuf();
        return OpaaxString(lBuffer.str().c_str());
    }

    bool WriteText(const OpaaxString& InAbsPath, const OpaaxString& InText)
    {
        try
        {
            const fs::path lPath(InAbsPath.CStr());
            if (lPath.has_parent_path()) { fs::create_directories(lPath.parent_path()); }

            std::ofstream lFile(InAbsPath.CStr());
            if (!lFile.is_open()) { return false; }
            lFile << InText.CStr();
            return true;
        }
        catch (const std::exception&)
        {
            return false;
        }
    }
}
