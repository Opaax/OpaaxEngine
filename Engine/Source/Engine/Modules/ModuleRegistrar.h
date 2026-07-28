#pragma once

#include <entt/entt.hpp>

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxStringID.hpp"
#include "Application/Services/ILogger.h"

#include "World/ComponentRegistry.h"

namespace Opaax
{
    inline constexpr LogCategory LogModuleRegistrar{"ModuleRegistrar"};

    // =============================================================================
    // ModuleRoute — a registration channel that still only RECORDS a count.
    //
    // Left for WorldSubsystems(), whose registry lands in M4. Keeping the skeleton means
    // the boot flow (engine natives -> game module -> editor module -> seal) stays
    // observable before the real machinery exists.
    // =============================================================================
    class OPAAX_API ModuleRoute
    {
    public:
        template<typename T>
        void Register() noexcept
        {
            // NOTE: records only a count. T is bound now so the call site is stable; the real
            // storage (WorldSubsystemRegistry) lands in M4.
            ++m_Count;
        }

        Uint64 Count() const noexcept { return m_Count; }

    private:
        Uint64 m_Count = 0;
    };

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

            return m_Registry->Register<T>(InName.IsValid() ? InName : DeriveName<T>());
        }

        /** Wire this route to the live registry. Called once, before any module registers. */
        void Bind(ComponentRegistry* InRegistry) noexcept { m_Registry = InRegistry; }

        /**
         * How many times a module asked — refusals included, so a mismatch with the
         * registry's own Count() is visible rather than inferred.
         */
        Uint64 Count() const noexcept { return m_Count; }

        // =========================================================================
        // Functions
        // =========================================================================
    private:
        /** The C++ type's leaf name: "Opaax::DummyComponent" -> "DummyComponent". */
        template<typename T>
        static OpaaxStringID DeriveName()
        {
            constexpr std::string_view lFullName = entt::type_name<T>::value();

            const std::size_t      lSeparator = lFullName.rfind("::");
            const std::string_view lLeaf      = (lSeparator == std::string_view::npos)
                                                    ? lFullName
                                                    : lFullName.substr(lSeparator + 2);

            return OpaaxStringID(OpaaxString(std::string(lLeaf).c_str()));
        }

        // =========================================================================
        // Members
        // =========================================================================
    private:
        ComponentRegistry* m_Registry = nullptr; // non-owning; the engine owns it (I5)
        Uint64             m_Count    = 0;
    };

    // =============================================================================
    // ModuleRegistrar — the single object a game module registers INTO (Editor.md D9).
    //   Handed to OpaaxApplication::RegisterModules between Bootstrap and EngineStartup:
    //   the engine registries exist (post-BootEngine), no world exists yet (pre-Startup).
    //     Components()      -> ComponentRegistry        (LIVE, M3)
    //     WorldSubsystems() -> WorldSubsystemRegistry   (M4)
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
        ComponentRoute&    Components()      noexcept { return m_Components; }
        ModuleRoute&       WorldSubsystems() noexcept { return m_WorldSubsystems; }

        const ComponentRoute& Components()      const noexcept { return m_Components; }
        const ModuleRoute&    WorldSubsystems() const noexcept { return m_WorldSubsystems; }

        /**
         * Point every live route at the engine's registries. Must run BEFORE the first
         * module registers, or registrations are dropped (loudly).
         */
        void BindEngineRegistries(ComponentRegistry& InComponents) noexcept
        {
            m_Components.Bind(&InComponents);
        }

    private:
        ComponentRoute m_Components;
        ModuleRoute    m_WorldSubsystems;
    };
}
