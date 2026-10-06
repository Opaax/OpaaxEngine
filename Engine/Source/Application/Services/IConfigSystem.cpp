#include "Application/Services/IConfigSystem.h"
#include "Application/Services/IPaths.h"

#include "Core/String/OpaaxUtf8.h"

#include <filesystem>

#include "Core/Log/Logger.h"

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
            void OnConfigRegistered(IConfig&) override {}   // defaults only
            bool SaveConfig(ConfigTypeID) override { return false; }
        };
    }

    // =========================================================================
    // Type tag + null object (defined here so there is one of each).
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
    // Registry
    // =========================================================================
    IConfig* IConfigSystem::FindConfig(const ConfigTypeID InId) const noexcept
    {
        for (const TUniquePtr<IConfig>& lConfig : m_Configs)
        {
            if (lConfig->GetConfigTypeID() == InId) { return lConfig.get(); }
        }

        return nullptr;
    }

    IConfig& IConfigSystem::FindOrCreate(const ConfigTypeID InId, const TFunction<TUniquePtr<IConfig>()>& InFactory)
    {
        if (IConfig* lFound = FindConfig(InId))
        {
            return *lFound;
        }

        m_Configs.emplace_back(InFactory());
        IConfig& lConfig = *m_Configs.back();

        // Registered before the hook, so a config reading others while loading finds itself.
        OnConfigRegistered(lConfig);

        return lConfig;
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
        
        // The directory may contain non-ASCII characters.
        return Utf8::FromFsPath(Utf8::ToFsPath(m_ConfigsDir) / InFileName);
    }

    void ConfigSystem::OnConfigRegistered(IConfig& InConfig)
    {
        // Loads the file, or creates the default one if missing.
        const OpaaxString lPath = JoinConfigPath(InConfig.FileName());

        // False means the file exists but could not be read: warn, since it runs on defaults.
        if (!InConfig.Load(lPath))
        {
            OPAAX_LOG(LogConfigSystem, Warn,
                      "Config [{}] could not be read — running on defaults. Fix or delete '{}'.",
                      InConfig.FileName(), lPath.CStr());
            return;
        }

        OPAAX_LOG(LogConfigSystem, Trace, "Config [{}] Created", InConfig.FileName());
    }

    bool ConfigSystem::SaveConfig(ConfigTypeID InId)
    {
        IConfig* lConfig = FindConfig(InId);
        return lConfig != nullptr && lConfig->Save();
    }

    void ConfigSystem::SaveAll()
    {
        for (const TUniquePtr<IConfig>& lConfig : GetConfigs())
        {
            lConfig->Save();
        }
    }
}
