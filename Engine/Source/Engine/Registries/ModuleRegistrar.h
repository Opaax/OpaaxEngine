#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxStringID.hpp"
#include "Core/Log/Logger.h"

#include "Core/Reflection/TypeInfo.h"   // DeriveTypeLeafName
#include "Engine/Registries/EngineRegistries.h"
#include "World/Behaviour/Behaviour.h"

namespace Opaax
{
    inline constexpr LogCategory LogModuleRegistrar{"ModuleRegistrar"};

    // =============================================================================
    // ComponentRoute — registers components into the ComponentRegistry.
    //   The name is optional (defaults to the type name) and is saved in map files:
    //   renaming the C++ type breaks saved maps unless the name is given explicitly.
    // =============================================================================
    class ComponentRoute
    {
        // =========================================================================
        // Registration
        // =========================================================================
    public:
        /**
         * Registers T as a component.
         * @tparam T Any type satisfying CComponent
         * @param InName Optional. Defaults to the type name ("QuadComponent").
         * @return True if registered
         */
        template<CComponent T>
        bool Register(OpaaxStringID InName = {})
        {
            static_assert(!CDerivedFrom<T, Behaviour>, "A behaviour is registered with Behaviours().Register<T>().");

            ++m_Count;

            if (m_Registry == nullptr)
            {
                // Not bound: BindEngineRegistries was never called.
                OPAAX_LOG(LogModuleRegistrar, Error,
                          "Components().Register — route is not bound to a ComponentRegistry; registration dropped.");
                return false;
            }

            return m_Registry->Register<T>(InName.IsValid() ? InName : DeriveTypeLeafName<T>());
        }

        /**
         * Registers T as a component every entity has (World::CreateEntity adds it; it cannot be
         * removed). The engine's TransformComponent is the only one.
         * @return True if registered
         */
        template<CComponent T>
        bool RegisterEssential(OpaaxStringID InName)
        {
            ++m_Count;

            if (m_Registry == nullptr)
            {
                OPAAX_LOG(LogModuleRegistrar, Error,
                          "Components().RegisterEssential — route is not bound to a ComponentRegistry; registration dropped.");
                return false;
            }

            return m_Registry->Register<T>(InName, /*bInEssential*/true);
        }

        /**
         * Lets files that still use an old component name load: InOldName resolves to the type
         * registered as InName. Register InName first.
         * @return True if the alias was added
         */
        bool AddAlias(OpaaxStringID InOldName, OpaaxStringID InName)
        {
            if (m_Registry == nullptr)
            {
                OPAAX_LOG(LogModuleRegistrar, Error,
                          "Components().AddAlias — route is not bound to a ComponentRegistry; alias dropped.");
                return false;
            }

            return m_Registry->AddAlias(InOldName, InName);
        }

        /** Connects this route to the registry. Called once, before any module registers. */
        void Bind(ComponentRegistry* InRegistry) noexcept { m_Registry = InRegistry; }

        /**
         * Number of registration requests, including refused ones.
         */
        Uint64 Count() const noexcept { return m_Count; }

        // =========================================================================
        // Members
        // =========================================================================
    private:
        ComponentRegistry* m_Registry = nullptr; // owned by the engine
        Uint64             m_Count    = 0;
    };

    // =============================================================================
    // BehaviourRoute — registers behaviours. A behaviour is stored and saved like a component, under
    //   its type name ("Coin", no namespace).
    // =============================================================================
    class BehaviourRoute
    {
        // =========================================================================
        // Registration
        // =========================================================================
    public:
        /**
         * Registers T as a behaviour.
         * @return True if registered
         */
        template<CBehaviour T>
        bool Register()
        {
            ++m_Count;

            if (m_Components == nullptr)
            {
                // Not bound: BindEngineRegistries was never called.
                OPAAX_LOG(LogModuleRegistrar, Error,
                          "Behaviours().Register — route is not bound to a ComponentRegistry; registration dropped.");
                return false;
            }

            return m_Components->Register<T>(DeriveTypeLeafName<T>());
        }

        /** Connects this route to the registries. Called once, before any module registers. */
        void Bind(ComponentRegistry* InComponents) noexcept { m_Components = InComponents; }

        /**
         * Number of registration requests, including refused ones.
         */
        Uint64 Count() const noexcept { return m_Count; }

        // =========================================================================
        // Members
        // =========================================================================
    private:
        ComponentRegistry* m_Components = nullptr; // owned by the engine
        Uint64             m_Count      = 0;
    };

    // =============================================================================
    // ResourceFormatRoute — registers resource types and their file extensions.
    //   The name is only used in logs and the editor.
    // =============================================================================
    class ResourceFormatRoute
    {
        // =========================================================================
        // Registration
        // =========================================================================
    public:
        /**
         * Registers T as a resource type, for the extensions its OPAAX_RESOURCE_FORMAT lists.
         * @tparam T A resource type with OPAAX_RESOURCE_FORMAT
         * @param InName Optional. Defaults to the type name.
         * @return True if registered
         */
        template<CResourceFormat T>
        bool Register(OpaaxStringID InName = {})
        {
            ++m_Count;

            if (m_Registry == nullptr)
            {
                // Not bound: BindEngineRegistries was never called.
                OPAAX_LOG(LogModuleRegistrar, Error,
                          "Resources().Register — route is not bound to a ResourceFormatRegistry; registration dropped.");
                return false;
            }

            return m_Registry->Register<T>(InName.IsValid() ? InName : DeriveTypeLeafName<T>());
        }

        /** Connects this route to the registry. Called once, before any module registers. */
        void Bind(ResourceFormatRegistry* InRegistry) noexcept { m_Registry = InRegistry; }

        /**
         * Number of registration requests, including refused ones.
         */
        Uint64 Count() const noexcept { return m_Count; }

        // =========================================================================
        // Members
        // =========================================================================
    private:
        ResourceFormatRegistry* m_Registry = nullptr; // owned by the engine
        Uint64                  m_Count    = 0;
    };

    // =============================================================================
    // ObjectClassRoute — registers the Object classes files can name. The saved name is the C++ class
    //   name (no namespace).
    // =============================================================================
    class ObjectClassRoute
    {
    public:
        /**
         * Registers T: files can then name it, and editors offer to make one.
         * @tparam T A class with OPAAX_OBJECT
         * @return True if registered
         */
        template<CObject T>
        bool Register()
        {
            ++m_Count;

            if (m_Registry == nullptr)
            {
                OPAAX_LOG(LogModuleRegistrar, Error,
                          "ObjectClasses().Register — route is not bound to an ObjectClassRegistry; registration dropped.");
                return false;
            }

            return m_Registry->Register<T>();
        }

        /**
         * Lets files that still name a class InOldName load as the class registered as InName.
         * @return True if the alias was added
         */
        bool AddAlias(const OpaaxStringID InOldName, const OpaaxStringID InName)
        {
            if (m_Registry == nullptr)
            {
                OPAAX_LOG(LogModuleRegistrar, Error,
                          "ObjectClasses().AddAlias — route is not bound to an ObjectClassRegistry; alias dropped.");
                return false;
            }

            return m_Registry->AddAlias(InOldName, InName);
        }

        /** Connects this route to the registry. Called once, before any module registers. */
        void Bind(ObjectClassRegistry* InRegistry) noexcept { m_Registry = InRegistry; }

        /** Number of registration requests, including refused ones. */
        Uint64 Count() const noexcept { return m_Count; }

    private:
        ObjectClassRegistry* m_Registry = nullptr; // owned by the engine
        Uint64               m_Count    = 0;
    };

    // =============================================================================
    // ModuleRegistrar — what engine and game code register into, before any world exists.
    //     Components()             -> ComponentRegistry
    //     Behaviours()             -> ComponentRegistry (stored like components)
    //     Resources()              -> ResourceFormatRegistry
    //     ObjectClasses()          -> ObjectClassRegistry (assets, subsystems, any Object class)
    //   Most types register themselves with the OPAAX_REGISTER_* macros (AutoRegistration.h).
    // =============================================================================
    class ModuleRegistrar
    {
    public:
        ComponentRoute&             Components()             noexcept { return m_Components; }
        BehaviourRoute&             Behaviours()             noexcept { return m_Behaviours; }
        ResourceFormatRoute&        Resources()              noexcept { return m_ResourceFormats; }
        ObjectClassRoute&           ObjectClasses()          noexcept { return m_ObjectClasses; }

        const ComponentRoute&             Components()             const noexcept { return m_Components; }
        const BehaviourRoute&             Behaviours()             const noexcept { return m_Behaviours; }
        const ResourceFormatRoute&        Resources()              const noexcept { return m_ResourceFormats; }
        const ObjectClassRoute&           ObjectClasses()          const noexcept { return m_ObjectClasses; }

        /**
         * Connects every route to the engine registries. Must run before anything registers.
         */
        void BindEngineRegistries(EngineRegistries& InRegistries) noexcept
        {
            m_Components.Bind(&InRegistries.Components());
            m_Behaviours.Bind(&InRegistries.Components());
            m_ResourceFormats.Bind(&InRegistries.Resources());
            m_ObjectClasses.Bind(&InRegistries.ObjectClasses());
        }

    private:
        ComponentRoute             m_Components;
        BehaviourRoute             m_Behaviours;
        ResourceFormatRoute        m_ResourceFormats;
        ObjectClassRoute           m_ObjectClasses;
    };
}
