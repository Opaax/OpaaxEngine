#include "IEngine.h"

#include "Engine/GameInstance/GameInstanceManager.h"
#include "Engine/Subsystems/EventBus/EngineEventBus.h"
#include "Engine/Subsystems/Input/InputManager.h"
#include "Engine/Subsystems/Resources/ResourceManager.h"
#include "Renderer/DebugDraw.h"
#include "RHI/Framebuffer.h"
#include "RHI/Texture.h"       // the TUniquePtr<ITexture2D> deleter
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

            // No WorldManager to create into. Null is the honest answer and I3 covers the
            // caller: a host that ignores the return simply boots with no world, which every
            // world consumer already handles (BO4).
            World* FinishStartup(const WorldSpec&) override { return nullptr; }
            World* OpenLevel(const WorldSpec&)     override { return nullptr; }

            // No registries and no subsystems to create, so a "game" here would be an empty object
            // pretending to be a session. False is the honest answer, and it matches the null
            // world above: nothing was created, and the caller's own null handling runs.
            bool StartGame() override { return false; }
            bool EndGame()   override { return false; }

            void Loop()                   override {}
            void PresentBackbuffer()      override {}
            // Nothing renders, so a submitted view is dropped on the floor — the same inert answer
            // every other hook here gives.
            void SubmitRenderView(IRenderTarget&, const CameraView&, bool) override {}

            // No device to create on — a caller gets nullptr and its own null-handling runs.
            TUniquePtr<IFramebuffer> CreateFramebuffer(const FramebufferSpec&) override { return nullptr; }

            // Same answer, and it is what lets a headless test decode a texture: the resource loads,
            // Initialize finds no device, and the payload is a CPU image with no GPU handle.
            TUniquePtr<ITexture2D> CreateTexture(const void*, Uint32, Uint32, Int32) override { return nullptr; }

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

            GameInstanceManager& GetGameInstances() override
            {
                // Inert — never started, so IsGameRunning answers false forever. That is the
                // right answer for a host with no engine: "no game" rather than a dangling ref.
                static GameInstanceManager s_NullGameInstances;
                return s_NullGameInstances;
            }

            InputManager& GetInput() override
            {
                // Inert — nothing feeds it, so every key reads as up. That is the right answer for
                // a host with no engine: "nothing is held" rather than a dangling reference.
                static InputManager s_NullInput;
                return s_NullInput;
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
