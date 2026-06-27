#include "IConfigSystem.h"
#include "IPaths.h"

#include "Core/Log/OpaaxLog.h"

#include <filesystem>

namespace Opaax
{
    namespace
    {
        namespace fs = std::filesystem;

        // =====================================================================
        // NullConfigSystem — in-memory configs (defaults), never touches disk.
        // =====================================================================
        class NullConfigSystem final : public IConfigSystem
        {
        public:
            bool IsNull() const noexcept override { return true; }
            void SaveAll() override {}

        protected:
            IConfig& FindOrCreate(ConfigTypeID InId, const TFunction<UniquePtr<IConfig>()>& InFactory) override
            {
                if (const auto lIt = m_Configs.find(InId); lIt != m_Configs.end())
                {
                    return *lIt->second;
                }

                UniquePtr<IConfig> lConfig = InFactory(); // defaults, no Load (no project layout)
                IConfig&           lRef    = *lConfig;
                m_Configs[InId] = std::move(lConfig);
                return lRef;
            }

            bool SaveConfig(ConfigTypeID) override { return false; }

        private:
            UnorderedMap<ConfigTypeID, UniquePtr<IConfig>> m_Configs;
        };
    }

    // =========================================================================
    // Type tag + null object (out-of-line — one instance across the DLL/exe line).
    // =========================================================================
    ServiceTypeID IConfigSystem::StaticTypeID() noexcept
    {
        static const int s_Tag = 0;
        return reinterpret_cast<ServiceTypeID>(&s_Tag);
    }

    IConfigSystem& IConfigSystem::Null()
    {
        static NullConfigSystem s_Null;
        return s_Null;
    }

    // =========================================================================
    // ConfigSystem
    // =========================================================================
    ConfigSystem::ConfigSystem(const IPaths& InPaths)
        : m_ConfigsDir(InPaths.ConfigsDir())
    {
    }

    OpaaxString ConfigSystem::JoinConfigPath(const char* InFileName) const
    {
        if (m_ConfigsDir.IsEmpty())
        {
            return OpaaxString(InFileName);
        }
        
        const fs::path lPath = fs::path(m_ConfigsDir.CStr()) / InFileName;
        return OpaaxString(lPath.generic_string().c_str());
    }

    IConfig& ConfigSystem::FindOrCreate(ConfigTypeID InId, const TFunction<UniquePtr<IConfig>()>& InFactory)
    {
        if (const auto lIt = m_Configs.find(InId); lIt != m_Configs.end())
        {
            return *lIt->second;
        }

        UniquePtr<IConfig> lConfig = InFactory();
        IConfig&           lRef    = *lConfig;

        // IConfig::Load loads the file, or generates the default file if it is missing.
        lRef.Load(JoinConfigPath(lRef.FileName()));

        m_Configs[InId] = std::move(lConfig);
        return lRef;
    }

    bool ConfigSystem::SaveConfig(ConfigTypeID InId)
    {
        const auto lIt = m_Configs.find(InId);
        return (lIt != m_Configs.end()) && lIt->second->Save();
    }

    void ConfigSystem::SaveAll()
    {
        for (auto& [lId, lConfig] : m_Configs)
        {
            if (lConfig) { lConfig->Save(); }
        }
    }
}
