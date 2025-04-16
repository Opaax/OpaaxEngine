#include "../Include/OpaaxEngine.h"
#include <cstdlib>
#include <iostream>

using namespace OPAAX;

int main()
{
    Engine::Get().Init();

    try
    {
        Engine::Get().Run();
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}