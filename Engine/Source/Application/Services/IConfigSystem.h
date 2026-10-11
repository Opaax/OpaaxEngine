#pragma once

#include <type_traits>

#include "Application/Services/IAppService.h"
#include "Core/Log/Logger.h"

#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"
#include "Core/Config/IConfig.h"

namespace Opaax
{
    class IPaths;
    
    inline constexpr LogCategory LogConfigSystem{"ConfigSystem"};

    // =============================================================================
    // IConfigSystem — registry of IConfig blocks, one file each under <ProjectRoot>/Configs/.
    //   Get<T>() registers the config on first use, so it never returns null.
    //   Configs are kept in registration order (the editor's Config panel lists them).
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
         * Registers and loads a config type (creates the default file if missing).
         * @return The owned config
         */
        template<class T>
        requires std::is_base_of_v<IConfig, T>
        T& Register()
        {
            return static_cast<T&>(FindOrCreate(T::StaticTypeID(),
                [] { return TUniquePtr<IConfig>(MakeUnique<T>()); }));
        }
        
        /**
         * @return The config, registered on first use. Never null.
         */
        template<class T>
        requires std::is_base_of_v<IConfig, T>
        T& Get() { return Register<T>(); }

        /**
         * Saves T to its file.
         * @return False if T was never registered
         */
        template<class T>
        requires std::is_base_of_v<IConfig, T>
        bool Save() { return SaveConfig(T::StaticTypeID()); }

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
        const TDynArray<TUniquePtr<IConfig>>& GetConfigs() const noexcept { return m_Configs; }

        /** @return The registered config under InId, or null. */
        IConfig* FindConfig(ConfigTypeID InId) const noexcept;
        // End Get - Set
        // =============================================================================

        // =============================================================================
        // Registry primitives
        // =============================================================================
    protected:
        /** Finds a config, or creates and registers it. */
        IConfig& FindOrCreate(ConfigTypeID InId, const TFunction<TUniquePtr<IConfig>()>& InFactory);

        /** Called when a config is registered (the real system loads it from disk). */
        virtual void OnConfigRegistered(IConfig& InConfig) = 0;

        virtual bool SaveConfig(ConfigTypeID InId) = 0;

        // =============================================================================
        // Members
        // =============================================================================
    private:
        // Registration order. Few configs, so a linear scan is fine.
        TDynArray<TUniquePtr<IConfig>> m_Configs;
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
        void OnConfigRegistered(IConfig& InConfig) override;
        bool SaveConfig(ConfigTypeID InId) override;

        // =============================================================================
        // Members
        // =============================================================================
    private:
        OpaaxString JoinConfigPath(const char* InFileName) const;

        OpaaxString m_ConfigsDir;
    };
}
