#pragma once

#include <type_traits>

#include "IAppService.h"
#include "Core/OpaaxTypes.h"
#include "Core/OpaaxString.hpp"
#include "Core/Config/IConfig.h"

namespace Opaax
{
    class IPaths;

    // =============================================================================
    // IConfigSystem — type-keyed registry of IConfig blocks, one file each under
    // <ProjectRoot>/Configs/ (file-per-config). Get<T>() auto-registers on a miss
    // (IConfig::Load creates the default file), so it NEVER returns null.
    //
    // The Register/Get/Save templates sit on two type-erased virtuals (FindOrCreate /
    // SaveConfig) so the registry works polymorphically through the IAppService null object.
    // =============================================================================
    class OPAAX_API IConfigSystem : public IAppService
    {
        // =============================================================================
        // Base Implementation
        // =============================================================================
    public:
        OPAAX_SERVICE_TYPE(IConfigSystem)
        
        // =============================================================================
        // Functions
        // =============================================================================
        
        /**
         * Register + load (or create the default file for) a config type; returns the owned T.
         * @tparam T 
         * @return 
         */
        template<class T>
        requires std::is_base_of_v<IConfig, T>
        T& Register()
        {
            return static_cast<T&>(FindOrCreate(T::StaticTypeID(),
                [] { return UniquePtr<IConfig>(MakeUnique<T>()); }));
        }
        
        /**
         * 
         * @tparam T The registered T, else auto-registered. Never null (decision: defaults always exist).
         * @return 
         */
        template<class T>
        requires std::is_base_of_v<IConfig, T>
        T& Get() { return Register<T>(); }

        /**
         * Persist T back to its file. False if T was never registered.
         * @tparam T 
         * @return 
         */
        template<class T>
        requires std::is_base_of_v<IConfig, T>
        bool Save() { return SaveConfig(T::StaticTypeID()); }

        // Persist every registered config.
        virtual void SaveAll() = 0;

        //----- null object ----------------------------------------------------
        static IConfigSystem& Null();

        // =============================================================================
        // Registry primitives — type-erased, implemented by the concrete systems.
        // =============================================================================
    protected:
        virtual IConfig& FindOrCreate(ConfigTypeID InId, const TFunction<UniquePtr<IConfig>()>& InFactory) = 0;
        virtual bool     SaveConfig(ConfigTypeID InId) = 0;
    };

    // =============================================================================
    // ConfigSystem — registry backed by <ProjectRoot>/Configs/. Each config loads from
    // ConfigsDir()/FileName() (IConfig::Load generates the default file if missing).
    // =============================================================================
    class OPAAX_API ConfigSystem final : public IConfigSystem
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
        IConfig& FindOrCreate(ConfigTypeID InId, const TFunction<UniquePtr<IConfig>()>& InFactory) override;
        bool     SaveConfig(ConfigTypeID InId) override;

        // =============================================================================
        // Members
        // =============================================================================
    private:
        OpaaxString JoinConfigPath(const char* InFileName) const;

        OpaaxString                                    m_ConfigsDir;
        UnorderedMap<ConfigTypeID, UniquePtr<IConfig>> m_Configs;
    };
}
