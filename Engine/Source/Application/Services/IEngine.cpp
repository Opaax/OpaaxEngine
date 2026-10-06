#include "IEngine.h"

#include "Engine/GameInstance/GameInstanceManager.h"
#include "Engine/Subsystems/EventBus/EngineEventBus.h"
#include "Engine/Subsystems/Input/InputManager.h"
#include "Engine/Subsystems/Resources/ResourceManager.h"
#include "Renderer/DebugDraw.h"
#include "RHI/Framebuffer.h"
#include "RHI/Texture.h"
#include "World/WorldManager.h"
#include "Engine/Registries/EngineRegistries.h"

namespace Opaax
{
    namespace
    {
        // =====================================================================
        // NullEngine — used when no engine is provided. Does nothing; getters
        // return empty static subsystems.
        // =====================================================================
        class NullEngine final : public IEngine
        {
        public:
            bool IsNull() const noexcept override { return true; }

            bool Startup()                override { return true; }

            World* FinishStartup(const WorldSpec&) override { return nullptr; }
            World* OpenLevel(const WorldSpec&)     override { return nullptr; }
            void   RequestOpenLevel(const WorldSpec&) override {}

            bool StartGame() override { return false; }
            bool EndGame()   override { return false; }

            void Loop()                   override {}
            void PresentBackbuffer()      override {}
            void SubmitRenderView(IRenderTarget&, const CameraView&, bool, World*, bool) override {}
            void SubmitUICanvas(UICanvas&, IRenderTarget*, const CameraView*) override {}

            TUniquePtr<IFramebuffer> CreateFramebuffer(const FramebufferSpec&) override { return nullptr; }

            // Lets headless tests load textures as CPU images.
            TUniquePtr<ITexture2D> CreateTexture(const void*, Uint32, Uint32, Int32) override { return nullptr; }

            void Update(double)           override {}
            void FixedUpdate(double)      override {}
            void Render(double)           override {}
            void TearDown()               override {}
            void Shutdown()               override {}

            EngineRegistries& GetRegistries() override
            {
                static EngineRegistries s_NullRegistries;
                return s_NullRegistries;
            }

            ResourceManager& GetResources() override
            {
                static ResourceManager s_NullResources;
                return s_NullResources;
            }

            EngineEventBus& GetEngineEventBus() override
            {
                static EngineEventBus s_NullBus;
                return s_NullBus;
            }

            WorldManager& GetWorldManager() override
            {
                static WorldManager s_NullWorlds;
                return s_NullWorlds;
            }

            GameInstanceManager& GetGameInstances() override
            {
                static GameInstanceManager s_NullGameInstances;
                return s_NullGameInstances;
            }

            InputManager& GetInput() override
            {
                static InputManager s_NullInput;
                return s_NullInput;
            }

            DebugDraw& GetDebugDraw() override
            {
                static DebugDraw s_NullDebugDraw;
                return s_NullDebugDraw;
            }
        };
    }

    // =========================================================================
    // Type tag + null object (defined here so there is one of each).
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
