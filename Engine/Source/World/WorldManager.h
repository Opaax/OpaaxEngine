#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"
#include "Application/Services/ILogger.h"
#include "Engine/Subsystems/EngineSubsystem.h"
#include "World/ComponentRegistry.h"
#include "World/World.h"
#include "World/WorldEvents.h"

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
        /** Registers the engine's native component types — see RegisterNativeComponents. */
        WorldManager();
        ~WorldManager() override = default;

        // =========================================================================
        // Function
        // =========================================================================

        // =========================================================================
        // World Lifetime
    public:
        //Todo: OpaaxStringID
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

        /**
         * Every component type the engine and the loaded game module know about. Populated
         * before the first world exists (engine natives in the ctor, module types through
         * ModuleRegistrar) and SEALED by the first CreateWorld.
         */
        ComponentRegistry&       GetComponentRegistry() noexcept       { return m_Components; }
        const ComponentRegistry& GetComponentRegistry() const noexcept { return m_Components; }

        // End Getters
        // =========================================================================

        // =========================================================================
        // Override
        // =========================================================================
        //~Begin EngineSubsystemBase interface
    public:
        bool Startup()  override;

        /**
         * Destroys every remaining world THROUGH DestroyWorld,
         * So each one announces itself while the bus and its subscribers are all still alive.
         * Shutdown() is too late for that, which is exactly why this phase exists.
         */
        void TearDown() override;

        void Shutdown() override;
        //~End EngineSubsystemBase interface

        // =========================================================================
        // Functions
        // =========================================================================
    private:
        /**
         * The engine's own component types, registered FIRST so a game module can never
         * shadow one (MR2: engine natives -> game module -> editor module -> seal). Runs in
         * the ctor because the subsystem create-pass is the only point that is guaranteed to
         * precede RegisterModules.
         */
        void RegisterNativeComponents();

        // =========================================================================
        // Members
        // =========================================================================
    private:
        ComponentRegistry           m_Components;
        TDynArray<UniquePtr<World>> m_Worlds;
        World*                      m_ActiveWorld = nullptr; // non-owning; points into m_Worlds
        
        // =========================================================================
        // Events
        // =========================================================================
    public:
        FOnWorldCreated       OnWorldCreated;
        FOnWorldDestroyed     OnWorldDestroyed;
        FOnActiveWorldChanged OnActiveWorldChanged;
    };
}
