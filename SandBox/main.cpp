//#include <OpaaxEngine.h>
#include <Platform/OpaaxWindow.h>

#include "Renderer/OpaaxVulkanContext.h"

int main()
{
    // Engine::Get().Run();
    // Engine::Get().Shutdown();

    OPWindow window(800, 600, "OpaaxEngine_SandBox");

    OpaaxVulkanContext vkContext(window.GetGLFWindow());
    vkContext.Init();

    while (!window.ShouldClose())
    {
        window.PollEvents();
    }

    return 0;
}