#pragma once

#include <entt/entt.hpp>

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxStringID.hpp"
#include "Core/Log/Logger.h"

#include "Engine/Reflection/TypeName.h"   // DeriveTypeLeafName
#include "Engine/Registries/EngineRegistries.h"

namespace Opaax
{
    inline constexpr LogCategory LogModuleRegistrar{"ModuleRegistrar"};

    // =============================================================================
    // ComponentRoute — registers components into the ComponentRegistry.
    //   The name is optional (defaults to the type name) and is saved in map files:
    //   renaming the C++ type breaks saved maps unless the name is given explicitly.
    // =============================================================================
    class OPAAX_API ComponentRoute
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
    // WorldSubsystemRoute — registers world subsystems into the WorldSubsystemRegistry.
    //   The name is optional (defaults to the type name); it is only used in logs and the editor.
    // =============================================================================
    class OPAAX_API WorldSubsystemRoute
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
    class OPAAX_API MoverModeRoute
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
    class OPAAX_API ResourceFormatRoute
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
    class OPAAX_API DataAssetRoute
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
    // ModuleRegistrar — what a game module registers into, before any world exists.
    //     Components()      -> ComponentRegistry
    //     WorldSubsystems() -> WorldSubsystemRegistry
    //     Resources()       -> ResourceFormatRegistry
    //     MoverModes()      -> MoverModeRegistry
    //     DataAssets()      -> DataAssetTypeRegistry
    // =============================================================================
    class OPAAX_API ModuleRegistrar
    {
    public:
        ComponentRoute&      Components()      noexcept { return m_Components; }
        WorldSubsystemRoute& WorldSubsystems() noexcept { return m_WorldSubsystems; }
        ResourceFormatRoute& Resources()       noexcept { return m_ResourceFormats; }
        MoverModeRoute&      MoverModes()      noexcept { return m_MoverModes; }
        DataAssetRoute&      DataAssets()      noexcept { return m_DataAssets; }

        const ComponentRoute&      Components()      const noexcept { return m_Components; }
        const WorldSubsystemRoute& WorldSubsystems() const noexcept { return m_WorldSubsystems; }
        const ResourceFormatRoute& Resources()       const noexcept { return m_ResourceFormats; }
        const MoverModeRoute&      MoverModes()      const noexcept { return m_MoverModes; }
        const DataAssetRoute&      DataAssets()      const noexcept { return m_DataAssets; }

        /**
         * Connects every route to the engine registries. Must run before any module registers.
         */
        void BindEngineRegistries(EngineRegistries& InRegistries) noexcept
        {
            m_Components.Bind(&InRegistries.Components());
            m_WorldSubsystems.Bind(&InRegistries.WorldSubsystems());
            m_ResourceFormats.Bind(&InRegistries.Resources());
            m_MoverModes.Bind(&InRegistries.MoverModes());
            m_DataAssets.Bind(&InRegistries.DataAssets());
        }

    private:
        ComponentRoute      m_Components;
        WorldSubsystemRoute m_WorldSubsystems;
        ResourceFormatRoute m_ResourceFormats;
        MoverModeRoute      m_MoverModes;
        DataAssetRoute      m_DataAssets;
    };
}
