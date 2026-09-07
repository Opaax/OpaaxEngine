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
    //
    // MSVC's type_name is ELABORATED ("class Opaax::DummyComponent"). For a namespaced type the
    // "::" strip removes that keyword as a side effect, which is why it went unnoticed until a
    // GLOBAL-namespace type registered (M4 S5) and produced the key "class QuadBoundsSubsystem".
    // The keyword is therefore stripped explicitly, FIRST — a component's derived name is what
    // gets written into map files, so a stray prefix there is an on-disk key nobody can read back.
    // =============================================================================
    template<typename T>
    OpaaxStringID DeriveTypeLeafName()
    {
        OpaaxStringView lName = entt::type_name<T>::value();

        for (const OpaaxStringView lKeyword : {"class ", "struct ", "enum ", "union "})
        {
            if (lName.StartsWith(lKeyword))
            {
                lName.RemovePrefix(lKeyword.GetLength());
                break;
            }
        }

        const Int32 lSeparator = lName.FindLast("::");
        const OpaaxStringView lLeaf = (lSeparator < 0)
                                          ? lName
                                          : lName.SubString(static_cast<Uint32>(lSeparator) + 2);

        return OpaaxStringID(lLeaf.ToString());
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
                          "Components().Register — route is not bound to a ComponentRegistry; registration dropped.");
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
                          "WorldSubsystems().Register — route is not bound to a WorldSubsystemRegistry; registration dropped.");
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
    // MoverModeRoute — the MoverModes() channel, LIVE since ⑦-A P5b.
    //
    //   Forwards into the engine's MoverModeRegistry. A mode's name IS an on-disk key — a
    //   `.opaaxmovemode` writes it in its Mode field — so unlike a world subsystem, the name is
    //   REQUIRED and renaming it breaks assets that name it.
    // =============================================================================
    class OPAAX_API MoverModeRoute
    {
        // =========================================================================
        // Registration
        // =========================================================================
    public:
        /**
         * Register T as a movement behaviour a tuning asset may name.
         *
         * @tparam T Derives IMoverMode and is default-constructible (modes are STATELESS).
         * @param InName The id a `.opaaxmovemode` writes. Required — it is a file key.
         * @return true when the registry accepted it.
         */
        template<typename T>
        requires std::is_base_of_v<IMoverMode, T>
        bool Register(const OpaaxStringID InName)
        {
            ++m_Count;

            if (m_Registry == nullptr)
            {
                OPAAX_LOG(LogModuleRegistrar, Error,
                          "MoverModes().Register — route is not bound to a MoverModeRegistry; registration dropped.");
                return false;
            }

            return m_Registry->Register<T>(InName);
        }

        /** Wire this route to the live registry. Called once, before any module registers. */
        void Bind(MoverModeRegistry* InRegistry) noexcept { m_Registry = InRegistry; }

        /** How many times a module asked — refusals included. */
        Uint64 Count() const noexcept { return m_Count; }

        // =========================================================================
        // Members
        // =========================================================================
    private:
        MoverModeRegistry* m_Registry = nullptr; // non-owning; the engine owns it (I5)
        Uint64             m_Count    = 0;
    };

    // =============================================================================
    // ResourceFormatRoute — the Resources() channel.
    //
    //   Forwards into the engine's ResourceFormatRegistry: which resource type loads which file
    //   extensions. A game registering its own resource type is the reason this table is engine-wide
    //   rather than editor-side — the editor then only says what its icon is.
    //
    //   Like a world subsystem's and unlike a component's, this name is NOT an on-disk key: a file
    //   is matched by EXTENSION, so the name appears only in logs and editor UI and the C++ type is
    //   safe to rename.
    // =============================================================================
    class OPAAX_API ResourceFormatRoute
    {
        // =========================================================================
        // Registration
        // =========================================================================
    public:
        /**
         * Register T as a loadable resource type, claiming every extension its
         * OPAAX_RESOURCE_FORMAT names.
         *
         * @tparam T A resource type carrying OPAAX_RESOURCE_FORMAT (CResourceFormat).
         * @param InName Optional. Omitted, the type's leaf name is used.
         * @return true when the registry accepted it.
         */
        template<CResourceFormat T>
        bool Register(OpaaxStringID InName = {})
        {
            ++m_Count;

            if (m_Registry == nullptr)
            {
                // Unbound means EngineStartup never called BindEngineRegistries — a wiring bug that
                // would otherwise drop every module resource type without a word.
                OPAAX_LOG(LogModuleRegistrar, Error,
                          "Resources().Register — route is not bound to a ResourceFormatRegistry; registration dropped.");
                return false;
            }

            return m_Registry->Register<T>(InName.IsValid() ? InName : DeriveTypeLeafName<T>());
        }

        /** Wire this route to the live registry. Called once, before any module registers. */
        void Bind(ResourceFormatRegistry* InRegistry) noexcept { m_Registry = InRegistry; }

        /**
         * How many times a module asked — refusals included, so a mismatch with the registry's
         * own Count() is visible rather than inferred.
         */
        Uint64 Count() const noexcept { return m_Count; }

        // =========================================================================
        // Members
        // =========================================================================
    private:
        ResourceFormatRegistry* m_Registry = nullptr; // non-owning; the engine owns it (I5)
        Uint64                  m_Count    = 0;
    };

    // =============================================================================
    // ModuleRegistrar — the single object a game module registers INTO (Editor.md D9).
    //   Handed to OpaaxApplication::RegisterModules between Bootstrap and EngineStartup:
    //   the engine registries exist (post-BootEngine), no world exists yet (pre-Startup).
    //     Components()      -> ComponentRegistry        (LIVE, M3)
    //     WorldSubsystems() -> WorldSubsystemRegistry   (LIVE, M4)
    //     Resources()       -> ResourceFormatRegistry   (LIVE)
    //     MoverModes()      -> MoverModeRegistry        (LIVE, ⑦-A)
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
        ResourceFormatRoute& Resources()       noexcept { return m_ResourceFormats; }
        MoverModeRoute&      MoverModes()      noexcept { return m_MoverModes; }

        const ComponentRoute&      Components()      const noexcept { return m_Components; }
        const WorldSubsystemRoute& WorldSubsystems() const noexcept { return m_WorldSubsystems; }
        const ResourceFormatRoute& Resources()       const noexcept { return m_ResourceFormats; }
        const MoverModeRoute&      MoverModes()      const noexcept { return m_MoverModes; }

        /**
         * Point every live route at the engine's registries. Must run BEFORE the first
         * module registers, or registrations are dropped (loudly).
         */
        void BindEngineRegistries(EngineRegistries& InRegistries) noexcept
        {
            m_Components.Bind(&InRegistries.Components());
            m_WorldSubsystems.Bind(&InRegistries.WorldSubsystems());
            m_ResourceFormats.Bind(&InRegistries.Resources());
            m_MoverModes.Bind(&InRegistries.MoverModes());
        }

    private:
        ComponentRoute      m_Components;
        WorldSubsystemRoute m_WorldSubsystems;
        ResourceFormatRoute m_ResourceFormats;
        MoverModeRoute      m_MoverModes;
    };
}
