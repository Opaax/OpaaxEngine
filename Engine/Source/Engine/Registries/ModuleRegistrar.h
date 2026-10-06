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
            static_assert(!std::derived_from<T, Behaviour>, "A behaviour is registered with Behaviours().Register<T>().");

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
    // WorldSubsystemRoute — registers world subsystems into the WorldSubsystemRegistry.
    //   The name is optional (defaults to the type name); it is only used in logs and the editor.
    // =============================================================================
    class WorldSubsystemRoute
    {
        // =========================================================================
        // Registration
        // =========================================================================
    public:
        /**
         * Registers T as a world subsystem. Each new world creates it unless T::ShouldCreate(const World&)
         * returns false.
         * @tparam T Derives IWorldSubsystem, constructible from WorldContext&
         * @param InName Optional. Defaults to the type name.
         * @return True if registered
         */
        template<typename T>
        requires std::is_base_of_v<IWorldSubsystem, T>
        bool Register(OpaaxStringID InName = {})
        {
            ++m_Count;

            if (m_Registry == nullptr)
            {
                // Not bound: BindEngineRegistries was never called.
                OPAAX_LOG(LogModuleRegistrar, Error,
                          "WorldSubsystems().Register — route is not bound to a WorldSubsystemRegistry; registration dropped.");
                return false;
            }

            return m_Registry->Register<T>(InName.IsValid() ? InName : DeriveTypeLeafName<T>());
        }

        /** Connects this route to the registry. Called once, before any module registers. */
        void Bind(WorldSubsystemRegistry* InRegistry) noexcept { m_Registry = InRegistry; }

        /**
         * Number of registration requests, including refused ones.
         */
        Uint64 Count() const noexcept { return m_Count; }

        // =========================================================================
        // Members
        // =========================================================================
    private:
        WorldSubsystemRegistry* m_Registry = nullptr; // owned by the engine
        Uint64                  m_Count    = 0;
    };

    // =============================================================================
    // MoverModeRoute — registers mover modes into the MoverModeRegistry.
    //   The name is required: .opaaxmovemode files store it.
    // =============================================================================
    class MoverModeRoute
    {
        // =========================================================================
        // Registration
        // =========================================================================
    public:
        /**
         * Registers T as a movement mode.
         * @tparam T Derives IMoverMode, default-constructible (stateless)
         * @param InName The name .opaaxmovemode files use. Required.
         * @return True if registered
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

        /** Connects this route to the registry. Called once, before any module registers. */
        void Bind(MoverModeRegistry* InRegistry) noexcept { m_Registry = InRegistry; }

        /** Number of registration requests, including refused ones. */
        Uint64 Count() const noexcept { return m_Count; }

        // =========================================================================
        // Members
        // =========================================================================
    private:
        MoverModeRegistry* m_Registry = nullptr; // owned by the engine
        Uint64             m_Count    = 0;
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
    // DataAssetRoute — registers the struct types a .opaaxdata can hold. The saved name is the
    //   C++ type name (no namespace).
    // =============================================================================
    class DataAssetRoute
    {
    public:
        /**
         * Registers T as a data asset type: it can then be created, edited and loaded as a .opaaxdata.
         * @tparam T A struct with OPAAX_PROPERTIES and NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT
         * @return True if registered
         */
        template<CDataAsset T>
        bool Register()
        {
            ++m_Count;

            if (m_Registry == nullptr)
            {
                OPAAX_LOG(LogModuleRegistrar, Error,
                          "DataAssets().Register — route is not bound to a DataAssetTypeRegistry; registration dropped.");
                return false;
            }

            return m_Registry->Register<T>();
        }

        /** Connects this route to the registry. Called once, before any module registers. */
        void Bind(DataAssetTypeRegistry* InRegistry) noexcept { m_Registry = InRegistry; }

        /** Number of registration requests, including refused ones. */
        Uint64 Count() const noexcept { return m_Count; }

    private:
        DataAssetTypeRegistry* m_Registry = nullptr; // owned by the engine
        Uint64                 m_Count    = 0;
    };

    // =============================================================================
    // GameInstanceSubsystemRoute — registers subsystems every game session creates.
    //   The name is optional (defaults to the type name); it is only used in logs.
    // =============================================================================
    class GameInstanceSubsystemRoute
    {
    public:
        /**
         * Registers T as a game instance subsystem (one per game session, created in registration order).
         * @tparam T Derives IGameInstanceSubsystem, constructible from GameInstanceContext&
         * @return True if registered
         */
        template<typename T>
        requires std::is_base_of_v<IGameInstanceSubsystem, T>
        bool Register(OpaaxStringID InName = {})
        {
            ++m_Count;

            if (m_Registry == nullptr)
            {
                OPAAX_LOG(LogModuleRegistrar, Error,
                          "GameInstanceSubsystems().Register — route is not bound; registration dropped.");
                return false;
            }

            return m_Registry->Register<T>(InName.IsValid() ? InName : DeriveTypeLeafName<T>());
        }

        /** Connects this route to the registry. Called once, before any module registers. */
        void Bind(GameInstanceSubsystemRegistry* InRegistry) noexcept { m_Registry = InRegistry; }

        /** Number of registration requests, including refused ones. */
        Uint64 Count() const noexcept { return m_Count; }

    private:
        GameInstanceSubsystemRegistry* m_Registry = nullptr; // owned by the engine
        Uint64                         m_Count    = 0;
    };

    // =============================================================================
    // UIWidgetRoute — registers the widget types a .opaaxui can use. The name is saved in the file.
    // =============================================================================
    class UIWidgetRoute
    {
    public:
        /**
         * Registers T as a widget type.
         * @param InName The name .opaaxui files use. Optional: defaults to the type name.
         * @return True if registered
         */
        template<typename T>
        requires std::is_base_of_v<UIWidget, T>
        bool Register(OpaaxStringID InName = {})
        {
            ++m_Count;

            if (m_Registry == nullptr)
            {
                OPAAX_LOG(LogModuleRegistrar, Error, "UIWidgets().Register — route is not bound; registration dropped.");
                return false;
            }

            return m_Registry->Register<T>(InName.IsValid() ? InName : DeriveTypeLeafName<T>());
        }

        /** Connects this route to the registry. Called once, before any module registers. */
        void Bind(UIWidgetRegistry* InRegistry) noexcept { m_Registry = InRegistry; }

        /** Number of registration requests, including refused ones. */
        Uint64 Count() const noexcept { return m_Count; }

    private:
        UIWidgetRegistry* m_Registry = nullptr; // owned by the engine
        Uint64            m_Count    = 0;
    };

    // =============================================================================
    // ModuleRegistrar — what engine and game code register into, before any world exists.
    //     Components()             -> ComponentRegistry
    //     Behaviours()             -> ComponentRegistry (stored like components)
    //     WorldSubsystems()        -> WorldSubsystemRegistry
    //     GameInstanceSubsystems() -> GameInstanceSubsystemRegistry
    //     Resources()              -> ResourceFormatRegistry
    //     MoverModes()             -> MoverModeRegistry
    //     DataAssets()             -> DataAssetTypeRegistry
    //     UIWidgets()              -> UIWidgetRegistry
    //   Most types register themselves with the OPAAX_REGISTER_* macros (AutoRegistration.h).
    // =============================================================================
    class ModuleRegistrar
    {
    public:
        ComponentRoute&             Components()             noexcept { return m_Components; }
        BehaviourRoute&             Behaviours()             noexcept { return m_Behaviours; }
        WorldSubsystemRoute&        WorldSubsystems()        noexcept { return m_WorldSubsystems; }
        GameInstanceSubsystemRoute& GameInstanceSubsystems() noexcept { return m_GameInstanceSubsystems; }
        ResourceFormatRoute&        Resources()              noexcept { return m_ResourceFormats; }
        MoverModeRoute&             MoverModes()             noexcept { return m_MoverModes; }
        DataAssetRoute&             DataAssets()             noexcept { return m_DataAssets; }
        UIWidgetRoute&              UIWidgets()              noexcept { return m_UIWidgets; }

        const ComponentRoute&             Components()             const noexcept { return m_Components; }
        const BehaviourRoute&             Behaviours()             const noexcept { return m_Behaviours; }
        const WorldSubsystemRoute&        WorldSubsystems()        const noexcept { return m_WorldSubsystems; }
        const GameInstanceSubsystemRoute& GameInstanceSubsystems() const noexcept { return m_GameInstanceSubsystems; }
        const ResourceFormatRoute&        Resources()              const noexcept { return m_ResourceFormats; }
        const MoverModeRoute&             MoverModes()             const noexcept { return m_MoverModes; }
        const DataAssetRoute&             DataAssets()             const noexcept { return m_DataAssets; }
        const UIWidgetRoute&              UIWidgets()              const noexcept { return m_UIWidgets; }

        /**
         * Connects every route to the engine registries. Must run before anything registers.
         */
        void BindEngineRegistries(EngineRegistries& InRegistries) noexcept
        {
            m_Components.Bind(&InRegistries.Components());
            m_Behaviours.Bind(&InRegistries.Components());
            m_WorldSubsystems.Bind(&InRegistries.WorldSubsystems());
            m_GameInstanceSubsystems.Bind(&InRegistries.GameInstanceSubsystems());
            m_ResourceFormats.Bind(&InRegistries.Resources());
            m_MoverModes.Bind(&InRegistries.MoverModes());
            m_DataAssets.Bind(&InRegistries.DataAssets());
            m_UIWidgets.Bind(&InRegistries.UIWidgets());
        }

    private:
        ComponentRoute             m_Components;
        BehaviourRoute             m_Behaviours;
        WorldSubsystemRoute        m_WorldSubsystems;
        GameInstanceSubsystemRoute m_GameInstanceSubsystems;
        ResourceFormatRoute        m_ResourceFormats;
        MoverModeRoute             m_MoverModes;
        DataAssetRoute             m_DataAssets;
        UIWidgetRoute              m_UIWidgets;
    };
}
