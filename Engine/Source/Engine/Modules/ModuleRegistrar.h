#pragma once

#include <entt/entt.hpp>

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxStringID.hpp"
#include "Application/Services/ILogger.h"

#include "Engine/Registries/EngineRegistries.h"

namespace Opaax
{
    inline constexpr LogCategory LogModuleRegistrar{"ModuleRegistrar"};

    // =============================================================================
    // DeriveTypeLeafName<T> — the C++ type's leaf name: "Opaax::DummyComponent" -> "DummyComponent".
    //
    // Shared by every route that accepts an optional authoring name. Lives here because this is
    // the header that already includes entt; the registries themselves take a REQUIRED name so
    // they need no entt of their own.
    // =============================================================================
    template<typename T>
    OpaaxStringID DeriveTypeLeafName()
    {
        constexpr std::string_view lFullName = entt::type_name<T>::value();

        const std::size_t      lSeparator = lFullName.rfind("::");
        const std::string_view lLeaf      = (lSeparator == std::string_view::npos)
                                                ? lFullName
                                                : lFullName.substr(lSeparator + 2);

        return OpaaxStringID(OpaaxString(std::string(lLeaf).c_str()));
    }

    // =============================================================================
    // ComponentRoute — the Components() channel, LIVE since M3.
    //
    //   Forwards straight into the engine's ComponentRegistry. The call site is unchanged
    //   from the M0 skeleton (MR1): `InRegistrar.Components().Register<TransformComponent>()`
    //   still compiles, because the authoring name is OPTIONAL and derived from the type
    //   when omitted.
    //
    //   On the derived name: it is the C++ type's leaf name, and it becomes the key written
    //   into map files. Renaming the C++ type therefore orphans components already saved
    //   under the old name — pass an explicit name to pin it. This is not silent when it
    //   happens: MapFactory warns per unknown component as it skips them.
    // =============================================================================
    class OPAAX_API ComponentRoute
    {
        // =========================================================================
        // Registration
        // =========================================================================
    public:
        /**
         * Register T as a serializable component.
         *
         * @tparam T Any type satisfying CComponent — no base class, no engine boilerplate.
         * @param InName Optional. Omitted, the type's leaf name is used ("DummyComponent").
         * @return true when the registry accepted it.
         */
        template<CComponent T>
        bool Register(OpaaxStringID InName = {})
        {
            ++m_Count;

            if (m_Registry == nullptr)
            {
                // Unbound means EngineStartup never called BindEngineRegistries — a wiring
                // bug that would otherwise drop every module component without a word.
                OPAAX_LOG(LogModuleRegistrar, Error,
                          "Components().Register — route is not bound to a ComponentRegistry; registration dropped.")
                return false;
            }

            return m_Registry->Register<T>(InName.IsValid() ? InName : DeriveTypeLeafName<T>());
        }

        /** Wire this route to the live registry. Called once, before any module registers. */
        void Bind(ComponentRegistry* InRegistry) noexcept { m_Registry = InRegistry; }

        /**
         * How many times a module asked — refusals included, so a mismatch with the
         * registry's own Count() is visible rather than inferred.
         */
        Uint64 Count() const noexcept { return m_Count; }

        // =========================================================================
        // Members
        // =========================================================================
    private:
        ComponentRegistry* m_Registry = nullptr; // non-owning; the engine owns it (I5)
        Uint64             m_Count    = 0;
    };

    // =============================================================================
    // WorldSubsystemRoute — the WorldSubsystems() channel, LIVE since M4.
    //
    //   Forwards into the engine's WorldSubsystemRegistry. The call site is unchanged from the
    //   M0 skeleton (MR1): `InRegistrar.WorldSubsystems().Register<WaveSpawnSubsystem>()` still
    //   compiles, because the authoring name is OPTIONAL and derived from the type when omitted.
    //
    //   Unlike a component name, this one is NOT an on-disk key — a candidate is identified by
    //   name only in logs and editor UI, so renaming the C++ type is safe here.
    // =============================================================================
    class OPAAX_API WorldSubsystemRoute
    {
        // =========================================================================
        // Registration
        // =========================================================================
    public:
        /**
         * Register T as a world-subsystem candidate — a type each new world may instantiate,
         * depending on T's optional static ShouldCreate(const World&).
         *
         * @tparam T Derives IWorldSubsystem and is constructible from WorldContext&.
         * @param InName Optional. Omitted, the type's leaf name is used.
         * @return true when the registry accepted it.
         */
        template<typename T>
        requires std::is_base_of_v<IWorldSubsystem, T>
        bool Register(OpaaxStringID InName = {})
        {
            ++m_Count;

            if (m_Registry == nullptr)
            {
                // Unbound means EngineStartup never called BindEngineRegistries — a wiring bug
                // that would otherwise drop every module subsystem without a word.
                OPAAX_LOG(LogModuleRegistrar, Error,
                          "WorldSubsystems().Register — route is not bound to a WorldSubsystemRegistry; registration dropped.")
                return false;
            }

            return m_Registry->Register<T>(InName.IsValid() ? InName : DeriveTypeLeafName<T>());
        }

        /** Wire this route to the live registry. Called once, before any module registers. */
        void Bind(WorldSubsystemRegistry* InRegistry) noexcept { m_Registry = InRegistry; }

        /**
         * How many times a module asked — refusals included, so a mismatch with the registry's
         * own Count() is visible rather than inferred.
         */
        Uint64 Count() const noexcept { return m_Count; }

        // =========================================================================
        // Members
        // =========================================================================
    private:
        WorldSubsystemRegistry* m_Registry = nullptr; // non-owning; the engine owns it (I5)
        Uint64                  m_Count    = 0;
    };

    // =============================================================================
    // ModuleRegistrar — the single object a game module registers INTO (Editor.md D9).
    //   Handed to OpaaxApplication::RegisterModules between Bootstrap and EngineStartup:
    //   the engine registries exist (post-BootEngine), no world exists yet (pre-Startup).
    //     Components()      -> ComponentRegistry        (LIVE, M3)
    //     WorldSubsystems() -> WorldSubsystemRegistry   (LIVE, M4)
    //
    //   ENGINE LAYER, not Application (moved M3). It exists to front the engine registries,
    //   and by I4's test a registrar that knows about component types knows about worlds.
    //   The M0 skeleton could live in Application only because it knew nothing — it counted.
    //   OpaaxApplication now holds it behind a forward declaration, so no Application header
    //   pulls in World/ or entt; composition roots include this header directly, which is
    //   exactly the reaching-across-layers a composition root is for (SE).
    // =============================================================================
    class OPAAX_API ModuleRegistrar
    {
    public:
        ComponentRoute&      Components()      noexcept { return m_Components; }
        WorldSubsystemRoute& WorldSubsystems() noexcept { return m_WorldSubsystems; }

        const ComponentRoute&      Components()      const noexcept { return m_Components; }
        const WorldSubsystemRoute& WorldSubsystems() const noexcept { return m_WorldSubsystems; }

        /**
         * Point every live route at the engine's registries. Must run BEFORE the first
         * module registers, or registrations are dropped (loudly).
         */
        void BindEngineRegistries(EngineRegistries& InRegistries) noexcept
        {
            m_Components.Bind(&InRegistries.Components());
            m_WorldSubsystems.Bind(&InRegistries.WorldSubsystems());
        }

    private:
        ComponentRoute      m_Components;
        WorldSubsystemRoute m_WorldSubsystems;
    };
}
