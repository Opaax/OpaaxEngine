#pragma once
#include <fstream>
#include <vector>

#include "OpaaxTypes.h"
#include "Core/OPLogMacro.h"

namespace OPAAX
{
    namespace SHADER_HELPER
    {
        static std::vector<char> ReadFile(const OString& Filename)
        {
            std::ifstream lFile(Filename, std::ios::ate | std::ios::binary);

            if (!lFile.is_open())
            {
                OPAAX_ERROR("[SHADER_HELPER] Failed to open file with name: %1%", %Filename)
                throw std::runtime_error("[SHADER_HELPER][ReadFile] Failed to open file!");
            }

            size_t lFileSize = (size_t) lFile.tellg();
            std::vector<char> lBuffer(lFileSize);

            lFile.seekg(0);
            lFile.read(lBuffer.data(), lFileSize);

            lFile.close();

            return lBuffer;
        }
    }
}
