#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/OpaaxString.hpp"
#include "Core/Application/Services/ILogger.h"
#include "Core/Engine/Subsystems/EngineSubsystem.h"
#include "Core/World/World.h"
#include "Core/World/WorldEvents.h"

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
        // Events (Tier-2)
        //
        // Bind to react to world lifetime; WorldManager itself knows no listeners. Every
        // Add/AddMember returns a DelegateHandle the listener MUST Remove (or RemoveAll
        // by owner) before it dies. Engine binds these and bridges them onto the
        // EngineEventBus for decoupled consumers — see WorldEvents.h.
        //
        // NOTE: a listener binding after Startup has missed the default "Main" world's
        // events; call GetActiveWorld() at bind time instead of assuming you saw it.
        // =========================================================================
    public:
        FOnWorldCreated       OnWorldCreated;
        FOnWorldDestroyed     OnWorldDestroyed;
        FOnActiveWorldChanged OnActiveWorldChanged;

        // =========================================================================
        // Function
        // =========================================================================

        // =========================================================================
        // World Lifetime
    public:
        World* CreateWorld(OpaaxString InName = "World");
        void   DestroyWorld(World* InWorld);
        // End World Lifetime
        // =========================================================================

        // =========================================================================
        // Getters
    public:
        World* GetActiveWorld() const noexcept { return m_ActiveWorld; }
        bool   SetActiveWorld(World* InWorld) noexcept;

        Uint64 GetWorldCount() const noexcept { return static_cast<Uint64>(m_Worlds.size()); }
        
        // End Getters
        // =========================================================================

        // =========================================================================
        // Override
        // =========================================================================
        //~Begin EngineSubsystemBase interface
    public:
        bool Startup()  override;
        void Shutdown() override;
        //~End EngineSubsystemBase interface
        
        // =========================================================================
        // Members
        // =========================================================================
    private:
        TDynArray<UniquePtr<World>> m_Worlds;
        World*                      m_ActiveWorld = nullptr; // non-owning; points into m_Worlds
    };
}
