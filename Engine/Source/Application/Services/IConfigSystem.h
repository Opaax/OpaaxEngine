#pragma once

#include "Core/Templates/TypeTraits.h"

#include "Application/Services/IAppService.h"
#include "Core/Log/Logger.h"

#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"
#include "Engine/Config/Config.h"
#include "Object/ObjectPtr.h"

namespace Opaax
{
    class IPaths;
    class ObjectClass;
    class ObjectClassRegistry;

    inline constexpr LogCategory LogConfigSystem{"ConfigSystem"};

    // =============================================================================
    // IConfigSystem — one config of each Config class, one file each under <ProjectRoot>/Configs/.
    //   Get<T>() registers the config on first use, so it never returns null; the engine registers the
    //   other classes at startup (RegisterClasses). Configs are kept in registration order (the
    //   editor's Config panel lists them).
    // =============================================================================
    class IConfigSystem : public IAppService
    {
        // =============================================================================
        // Base Implementation
        // =============================================================================
    public:
        OPAAX_SERVICE_TYPE(IConfigSystem)

        IConfigSystem() = default;

        // =============================================================================
        // Copy - Move : Delete
        // =============================================================================

        // Deleted explicitly: an exported class with a move-only member still instantiates its copy.
        IConfigSystem(const IConfigSystem&)            = delete;
        IConfigSystem& operator=(const IConfigSystem&) = delete;
        IConfigSystem(IConfigSystem&&)                 = delete;
        IConfigSystem& operator=(IConfigSystem&&)      = delete;

        // =============================================================================
        // Functions
        // =============================================================================

        /**
         * Registers and loads a config class (creates the default file if missing).
         * @return The owned config
         */
        template<class T>
        requires CDerivedFrom<T, Config>
        T& Register()
        {
            static_assert(!TIsAbstract<T>, "IConfigSystem::Register: T must be a concrete config class.");
            return static_cast<T&>(FindOrCreate(T::StaticClass()));
        }

        /**
         * @return The config, registered on first use. Never null.
         */
        template<class T>
        requires CDerivedFrom<T, Config>
        T& Get() { return Register<T>(); }

        /**
         * Saves T to its file.
         * @return False if T was never registered
         */
        template<class T>
        requires CDerivedFrom<T, Config>
        bool Save() { return SaveConfig(T::StaticClass()); }

        /**
         * Registers every concrete Config class of InClasses not registered yet, in their order: the
         * Config panel lists a game's configs before its code first reads them.
         */
        void RegisterClasses(const ObjectClassRegistry& InClasses);

        // Saves every registered config.
        virtual void SaveAll() = 0;

        //----- null object ----------------------------------------------------
        static IConfigSystem& Null();

        // =============================================================================
        // Get - Set
    public:
        /**
         * Every registered config, in registration order.
         * Can grow at any time (Get<T>() registers), so don't cache it.
         */
        const TDynArray<TStrongObjectPtr<Config>>& GetConfigs() const noexcept { return m_Configs; }

        /** @return The registered config of InClass, or null. */
        Config* FindConfig(const ObjectClass& InClass) const noexcept;
        // End Get - Set
        // =============================================================================

        // =============================================================================
        // Registry primitives
        // =============================================================================
    protected:
        /** Finds the config of InClass, a concrete Config class, or makes and registers it. */
        Config& FindOrCreate(const ObjectClass& InClass);

        /** Called when a config is registered (the real system loads it from disk). */
        virtual void OnConfigRegistered(Config& InConfig) = 0;

        virtual bool SaveConfig(const ObjectClass& InClass) = 0;

        // =============================================================================
        // Members
        // =============================================================================
    private:
        // Registration order. Few configs, so a linear scan is fine.
        TDynArray<TStrongObjectPtr<Config>> m_Configs;
    };

    // =============================================================================
    // ConfigSystem — registry backed by <ProjectRoot>/Configs/.
    // =============================================================================
    class ConfigSystem final : public IConfigSystem
    {
        // =============================================================================
        // CTOR
        // =============================================================================
    public:
        explicit ConfigSystem(const IPaths& InPaths);

        // =============================================================================
        // Copy - Move : Delete
        // =============================================================================
        ConfigSystem(const ConfigSystem&)            = delete;
        ConfigSystem& operator=(const ConfigSystem&) = delete;
        ConfigSystem(ConfigSystem&&)                 = delete;
        ConfigSystem& operator=(ConfigSystem&&)      = delete;

        // =============================================================================
        // Override
        // =============================================================================
    public:
        void SaveAll() override;

    protected:
        void OnConfigRegistered(Config& InConfig) override;
        bool SaveConfig(const ObjectClass& InClass) override;

        // =============================================================================
        // Members
        // =============================================================================
    private:
        OpaaxString JoinConfigPath(const char* InFileName) const;

        OpaaxString m_ConfigsDir;
    };
}
