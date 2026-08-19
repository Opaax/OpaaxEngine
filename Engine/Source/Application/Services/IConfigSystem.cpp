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
            void OnConfigRegistered(IConfig&) override {}   // defaults only — no project layout to load from
            bool SaveConfig(ConfigTypeID) override { return false; }
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
    // Registry — shared by both systems (the storage is the base's).
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

        // In the registry BEFORE the hook runs: a config that reads a sibling while loading must
        // find itself already registered rather than register a second copy.
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
        
        // InFileName is an ASCII config name; the DIRECTORY is the part that can carry non-ASCII (I7).
        return Utf8::FromFsPath(Utf8::ToFsPath(m_ConfigsDir) / InFileName);
    }

    void ConfigSystem::OnConfigRegistered(IConfig& InConfig)
    {
        // IConfig::Load loads the file, or generates the default file if it is missing.
        InConfig.Load(JoinConfigPath(InConfig.FileName()));
        OPAAX_LOG(LogConfigSystem, Info, "Config [{}] Created", InConfig.FileName());
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
