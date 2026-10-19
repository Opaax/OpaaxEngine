#include "Core/IO/Console.h"

#include <cstdio>

namespace Opaax::Console
{
    void WriteLine(const OpaaxStringView InText)
    {
        if (!InText.IsEmpty())
        {
            std::fwrite(InText.Data(), 1, InText.GetLength(), stdout);
        }
        std::fputc('\n', stdout);
        std::fflush(stdout);
    }
}
