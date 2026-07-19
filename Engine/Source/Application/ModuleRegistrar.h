#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"

namespace Opaax
{
    // =============================================================================
    // ModuleRoute — one registration channel exposed by ModuleRegistrar.
    //
    // M0 SKELETON: Register<T>() only records that a type was offered (a count), so the
    // boot flow (engine natives -> game module -> editor module -> seal) is observable and
    // testable before the real registries exist. The call-site API is final NOW:
    //   InRegistrar.Components().Register<TransformComponent>();
    //   InRegistrar.WorldSubsystems().Register<WaveSpawnSubsystem>();
    // Only the body changes later — M3 forwards Components() to ComponentRegistry v2, M4
    // forwards WorldSubsystems() to the single WorldSubsystemRegistry. No consumer edits.
    // =============================================================================
    class OPAAX_API ModuleRoute
    {
    public:
        template<typename T>
        void Register() noexcept
        {
            // NOTE: M0 records only a count. T is bound now so the call site is stable; the
            // real storage (ComponentRegistry v2 / WorldSubsystemRegistry) lands in M3 / M4.
            ++m_Count;
        }

        Uint64 Count() const noexcept { return m_Count; }

    private:
        Uint64 m_Count = 0;
    };

    // =============================================================================
    // ModuleRegistrar — the single object a game module registers INTO (Editor.md D9).
    //   Handed to OpaaxApplication::OnRegisterModules between Bootstrap and EngineStartup:
    //   the engine registries exist (post-BootEngine), no world exists yet (pre-Startup).
    //   Two routes, mirroring the engine registries they will front:
    //     Components()      -> ComponentRegistry v2      (M3)
    //     WorldSubsystems() -> WorldSubsystemRegistry    (M4)
    // =============================================================================
    class OPAAX_API ModuleRegistrar
    {
    public:
        ModuleRoute&       Components()      noexcept { return m_Components; }
        ModuleRoute&       WorldSubsystems() noexcept { return m_WorldSubsystems; }

        const ModuleRoute& Components()      const noexcept { return m_Components; }
        const ModuleRoute& WorldSubsystems() const noexcept { return m_WorldSubsystems; }

    private:
        ModuleRoute m_Components;
        ModuleRoute m_WorldSubsystems;
    };
}
