#include "IConfigSystem.h"
#include "IPaths.h"

#include "Core/String/OpaaxUtf8.h"   // I7

#include <filesystem>

#include "ILogger.h"

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
            IConfig& FindOrCreate(ConfigTypeID InId, const TFunction<TUniquePtr<IConfig>()>& InFactory) override
            {
                if (const auto lIt = m_Configs.find(InId); lIt != m_Configs.end())
                {
                    return *lIt->second;
                }

                TUniquePtr<IConfig> lConfig = InFactory(); // defaults, no Load (no project layout)
                IConfig&           lRef    = *lConfig;
                m_Configs[InId] = std::move(lConfig);
                return lRef;
            }

            bool SaveConfig(ConfigTypeID) override { return false; }

        private:
            TUnorderedMap<ConfigTypeID, TUniquePtr<IConfig>> m_Configs;
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
        
        // InFileName is an ASCII config name; the DIRECTORY is the part that can carry non-ASCII (I7).
        return Utf8::FromFsPath(Utf8::ToFsPath(m_ConfigsDir) / InFileName);
    }

    IConfig& ConfigSystem::FindOrCreate(ConfigTypeID InId, const TFunction<TUniquePtr<IConfig>()>& InFactory)
    {
        if (const auto lIt = m_Configs.find(InId); lIt != m_Configs.end())
        {
            return *lIt->second;
        }

        TUniquePtr<IConfig> lConfig = InFactory();
        IConfig&           lRef    = *lConfig;

        // IConfig::Load loads the file, or generates the default file if it is missing.
        lRef.Load(JoinConfigPath(lRef.FileName()));
        OPAAX_LOG(LogConfigSystem, Info, "Config [{}] Created", lRef.FileName());

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
