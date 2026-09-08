#pragma once

#include "Core/EngineAPI.h"

#include "Engine/GameInstance/GameInstanceSubsystemRegistry.h"
#include "Engine/Subsystems/Resources/ResourceFormatRegistry.h"
#include "World/Components/ComponentRegistry.h"
#include "World/Systems/Movement/MoverModeRegistry.h"
#include "World/Systems/WorldSubsystemRegistry.h"

namespace Opaax
{
    // =============================================================================
    // EngineRegistries — every "what types exist" table the engine owns, in one place.
    //
    //   WHY ENGINE-LEVEL, not on the subsystem that consumes each one (Editor.md §2, which
    //   has said "Engine builds the registry" since v3):
    //     - A registry is TYPE METADATA, not the state of any one subsystem. ComponentRegistry
    //       already has two consumers — the snapshot core and (M5) the Inspector's Add
    //       Component menu — and neither is "the world manager".
    //     - There is more than one. WorldSubsystemRegistry lands in M4. Hanging each off
    //       whichever subsystem happens to read it turns that subsystem into a bag, and makes
    //       the module-registrar binding grow an argument from a different owner every time.
    //     - Registration is a BOOT-ORDER concern (MR2: natives -> game module -> editor module
    //       -> seal -> first world), and boot order is the Engine's business.
    //
    //   Owned BY VALUE by Engine. Consumers borrow a non-owning pointer/reference (I5).
    //   ModuleRegistrar is the only thing that WRITES here, and only before the seal.
    // =============================================================================
    class OPAAX_API EngineRegistries
    {
        // =========================================================================
        // CTORS - DTORS
        // =========================================================================
    public:
        EngineRegistries()  = default;
        ~EngineRegistries() = default;

        // =========================================================================
        // Copy - Move Delete
        // =========================================================================
        //
        // Required, not hygiene: OPAAX_API instantiates every implicitly-declared member, and
        // the registries below are non-copyable (I6 corollary).
        EngineRegistries(const EngineRegistries&)            = delete;
        EngineRegistries& operator=(const EngineRegistries&) = delete;
        EngineRegistries(EngineRegistries&&)                 = delete;
        EngineRegistries& operator=(EngineRegistries&&)      = delete;

        // =========================================================================
        // Get - Set
        // =========================================================================
    public:
        ComponentRegistry&       Components()       noexcept { return m_Components; }
        const ComponentRegistry& Components() const noexcept { return m_Components; }

        // The reason this aggregate exists — a second registry, added in M4 without giving
        // anything a new owner or widening BindEngineRegistries' signature.
        WorldSubsystemRegistry&       WorldSubsystems()       noexcept { return m_WorldSubsystems; }
        const WorldSubsystemRegistry& WorldSubsystems() const noexcept { return m_WorldSubsystems; }

        // Which resource type loads a given file extension. Read by the editor's browser today and
        // by the manager when loading by path lands — neither of which is "the ResourceManager's
        // state", which is why it sits here with its siblings rather than on that subsystem.
        ResourceFormatRegistry&       Resources()       noexcept { return m_ResourceFormats; }
        const ResourceFormatRegistry& Resources() const noexcept { return m_ResourceFormats; }

        // ⑦-A P5b — which behaviours a `.opaaxmovemode` may name. Here for the reason above and
        // not on MoverSubsystem: a world subsystem is created per world, and the modes are one set.
        MoverModeRegistry&       MoverModes()       noexcept { return m_MoverModes; }
        const MoverModeRegistry& MoverModes() const noexcept { return m_MoverModes; }

        // ⑦-B — which subsystems a game SESSION runs. Here rather than on GameInstanceManager for
        // the reason above: the candidates are one set, and the instance that runs them is created
        // and destroyed repeatedly across a session's life (every PIE cycle).
        GameInstanceSubsystemRegistry&       GameInstanceSubsystems()       noexcept { return m_GameInstanceSubsystems; }
        const GameInstanceSubsystemRegistry& GameInstanceSubsystems() const noexcept { return m_GameInstanceSubsystems; }

        // =========================================================================
        // Functions
        // =========================================================================
    public:
        /**
         * Close every registry to further registration. Called on the way to the FIRST world
         * (Editor.md §3 L1) — after that, a late-registered type would be silently missing
         * from a world that already exists. Idempotent.
         */
        void SealAll() noexcept
        {
            m_Components.Seal();
            m_WorldSubsystems.Seal();
            m_ResourceFormats.Seal();
            m_MoverModes.Seal();
            m_GameInstanceSubsystems.Seal();
        }

        // =========================================================================
        // Members
        // =========================================================================
    private:
        ComponentRegistry             m_Components;
        WorldSubsystemRegistry        m_WorldSubsystems;
        ResourceFormatRegistry        m_ResourceFormats;
        MoverModeRegistry             m_MoverModes;
        GameInstanceSubsystemRegistry m_GameInstanceSubsystems;
    };
}
