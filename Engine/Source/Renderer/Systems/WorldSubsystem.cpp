#include "WorldSubsystem.h"

#include "Renderer/Systems/WorldRenderSystem.h"
#include "Core/Log/OpaaxLog.h"

namespace Opaax
{
    bool WorldSubsystem::Startup()
    {
        OPAAX_CORE_INFO("WorldSubsystem::Startup()");

        // Engine default world renderer first → game systems (appended from CoreEngineApp::OnStartup)
        // draw on top, in registration order.
        Register(MakeUnique<WorldRenderSystem>());
        return true;
    }

    void WorldSubsystem::Shutdown()
    {
        OPAAX_CORE_INFO("WorldSubsystem::Shutdown() — clearing {} world system(s).", m_RenderSystems.size());

        // Reverse-of-startup teardown: WorldSubsystem is registered AFTER RenderSubsystem, so this runs
        // BEFORE the render device is destroyed. Any GPU resource a system owns (e.g. a demo's textures)
        // is freed here while the device is alive — otherwise Vulkan/VMA asserts on leaked allocations.
        m_RenderSystems.clear();
    }

    void WorldSubsystem::Register(UniquePtr<IWorldSystem> InSystem)
    {
        if (InSystem)
        {
            m_RenderSystems.push_back(Move(InSystem));
        }
    }
}
