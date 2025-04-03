#include <Core/OPLogMacro.hpp>
#include <OpaaxEngine.h>

int main() {
    Engine::Get().Run();
    Engine::Get().Shutdown();

    return 0;
}