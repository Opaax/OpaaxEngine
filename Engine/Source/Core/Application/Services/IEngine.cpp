#include "IEngine.h"

#include "Core/Engine/Subsystems/EventBus/EngineEventBus.h"
#include "Core/Engine/Subsystems/Resources/ResourceManager.h"
#include "Core/World/WorldManager.h"

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
            void Update(double)           override {}
            void FixedUpdate(double)      override {}
            void Render(double)           override {}
            void Shutdown()               override {}

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
