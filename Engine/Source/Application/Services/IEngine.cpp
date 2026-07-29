#include "IEngine.h"

#include "Engine/Subsystems/EventBus/EngineEventBus.h"
#include "Engine/Subsystems/Resources/ResourceManager.h"
#include "Renderer/DebugDraw.h"
#include "RHI/Framebuffer.h"
#include "World/WorldManager.h"
#include "Engine/Registries/EngineRegistries.h"

namespace Opaax
{
    namespace
    {
        // =====================================================================
        // NullEngine — the locator's fallback when no engine is provided. Inert:
        // it starts/pumps nothing. GetResources() hands back a static, never-started
        // ResourceManager so callers never dereference a dangling reference.
        // =====================================================================
        class NullEngine final : public IEngine
        {
        public:
            bool IsNull() const noexcept override { return true; }

            bool Startup()                override { return true; }
            void Loop()                   override {}
            void PresentBackbuffer()      override {}
            void SetPrimaryRenderTarget(IRenderTarget*) override {}

            // No device to create on — a caller gets nullptr and its own null-handling runs.
            UniquePtr<IFramebuffer> CreateFramebuffer(const FramebufferSpec&) override { return nullptr; }

            void Update(double)           override {}
            void FixedUpdate(double)      override {}
            void Render(double)           override {}
            void TearDown()               override {}
            void Shutdown()               override {}

            EngineRegistries& GetRegistries() override
            {
                static EngineRegistries s_NullRegistries; // inert — nothing registers into it
                return s_NullRegistries;
            }

            ResourceManager& GetResources() override
            {
                static ResourceManager s_NullResources; // inert — never Startup()'d
                return s_NullResources;
            }

            EngineEventBus& GetEngineEventBus() override
            {
                static EngineEventBus s_NullBus; // inert — publishes reach no one
                return s_NullBus;
            }

            WorldManager& GetWorldManager() override
            {
                static WorldManager s_NullWorlds; // inert — owns no worlds
                return s_NullWorlds;
            }

            DebugDraw& GetDebugDraw() override
            {
                // Inert — no renderer exists to drain it, so anything enqueued here is simply never
                // drawn. Only reachable when NO engine was provided to the locator (a misconfigured
                // host or a test double), which is also why nothing loops on it in practice.
                static DebugDraw s_NullDebugDraw;
                return s_NullDebugDraw;
            }
        };
    }

    // =========================================================================
    // Type tag + null object (out-of-line — one instance across the DLL/exe line).
    // =========================================================================
    ServiceTypeID IEngine::StaticTypeID() noexcept
    {
        static const int s_Tag = 0;
        return reinterpret_cast<ServiceTypeID>(&s_Tag);
    }

    IEngine& IEngine::Null()
    {
        static NullEngine s_Null;
        return s_Null;
    }
}
