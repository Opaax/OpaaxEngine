#pragma once

#include <type_traits>

#include "IAppService.h"
#include "ILogger.h"

#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"
#include "Core/Config/IConfig.h"

namespace Opaax
{
    class IPaths;
    
    inline constexpr LogCategory LogConfigSystem{"ConfigSystem"};

    // =============================================================================
    // IConfigSystem — registry of IConfig blocks, one file each under <ProjectRoot>/Configs/
    // (file-per-config). Get<T>() auto-registers on a miss (IConfig::Load creates the default
    // file), so it NEVER returns null.
    //
    // The STORAGE lives here, in REGISTRATION ORDER: the registry is a list the editor's Config
    // panel walks (GetConfigs), and hash order would have made that list arbitrary. Registration
    // is shared; the two systems differ only in OnConfigRegistered (load from disk or not) and in
    // whether they save at all, which is what the null object has to refuse.
    // =============================================================================
    class OPAAX_API IConfigSystem : public IAppService
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

        // Owns the configs through TUniquePtr — an exported class holding a move-only member must
        // say so, or its implicit copy-assign is instantiated anyway (I6's corollary).
        IConfigSystem(const IConfigSystem&)            = delete;
        IConfigSystem& operator=(const IConfigSystem&) = delete;
        IConfigSystem(IConfigSystem&&)                 = delete;
        IConfigSystem& operator=(IConfigSystem&&)      = delete;

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
                [] { return TUniquePtr<IConfig>(MakeUnique<T>()); }));
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
        // Get - Set
    public:
        /**
         * Every registered config, in registration order — what the editor's Config panel lists.
         *
         * Read it LIVE: Get<T>() auto-registers, so a system reading its config on any later frame
         * grows this list. A caller that snapshots it will miss those.
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
        /** Registration itself is shared; what a NEW config then goes through is OnConfigRegistered. */
        IConfig& FindOrCreate(ConfigTypeID InId, const TFunction<TUniquePtr<IConfig>()>& InFactory);

        /** A config just entered the registry — the real system loads it from disk, the null one does not. */
        virtual void OnConfigRegistered(IConfig& InConfig) = 0;

        virtual bool SaveConfig(ConfigTypeID InId) = 0;

        // =============================================================================
        // Members
        // =============================================================================
    private:
        // Registration order, and a linear scan to find one: a handful of configs makes hashing the
        // slower of the two, and the order is what the panel lists.
        TDynArray<TUniquePtr<IConfig>> m_Configs;
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
