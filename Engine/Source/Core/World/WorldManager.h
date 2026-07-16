#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/OpaaxString.hpp"
#include "Core/Application/Services/ILogger.h"
#include "Core/Engine/Subsystems/EngineSubsystem.h"
#include "Core/World/World.h"

namespace Opaax
{
    inline constexpr LogCategory LogWorldManager{"WorldManager"};

    // =============================================================================
    // WorldManager — the engine subsystem that OWNS every World (UniquePtr). Multiple
    //   worlds may coexist (editor + PIE later); one is the "active" world the renderer
    //   draws. Creates a default world on Startup so there is always a render target.
    //   Ownership lives here; drivers hold non-owning World* handles.
    // =============================================================================
    class OPAAX_API WorldManager final : public EngineSubsystemBase
    {
        // =========================================================================
        // Base Implementation
        // =========================================================================
    public:
        OPAAX_SUBSYSTEM_TYPE(WorldManager)

        // =========================================================================
        // CTORS - DTORS
        // =========================================================================
    public:
        WorldManager()           = default;
        ~WorldManager() override = default;

        // =========================================================================
        // Override — EngineSubsystemBase
        // =========================================================================
    public:
        bool Startup()  override;
        void Shutdown() override;

        // =========================================================================
        // World lifetime
        // =========================================================================
    public:
        World* CreateWorld(OpaaxString InName = "World");
        void   DestroyWorld(World* InWorld);

        // =========================================================================
        // Active (render) world
        // =========================================================================
    public:
        World* GetActiveWorld() const noexcept { return m_ActiveWorld; }
        void   SetActiveWorld(World* InWorld) noexcept;

        Uint64 GetWorldCount() const noexcept { return static_cast<Uint64>(m_Worlds.size()); }

        // =========================================================================
        // Members
        // =========================================================================
    private:
        TDynArray<UniquePtr<World>> m_Worlds;
        World*                      m_ActiveWorld = nullptr; // non-owning; points into m_Worlds
    };
}
