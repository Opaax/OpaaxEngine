#include "Application/Services/IUserSettings.h"

#include "Application/Services/IConfigSystem.h"
#include "Application/Services/ISaveStorage.h"
#include "Application/Services/IWindowManager.h"
#include "Audio/AudioBusSettings.h"
#include "Core/Maths/Maths.h"
#include "Core/Reflection/OpaaxEnumJson.h"
#include "Core/String/OpaaxStringJson.h"
#include "Engine/Config/ConfigLayer.h"
#include "Engine/Config/Config_Engine.h"
#include "Object/ObjectClass.h"
#include "Platform/Window/Window.h"
#include "Save/SaveGameFile.h"

namespace Opaax
{
    namespace
    {
        // =====================================================================
        // NullUserSettings — keeps nothing.
        // =====================================================================
        class NullUserSettings final : public IUserSettings
        {
        public:
            bool           IsNull() const noexcept override { return true; }
            ESaveResult    Load() override { return ESaveResult::Missing; }
            ESaveResult    Save() override { return ESaveResult::IoError; }
            bool           HasUnsavedChanges() const override { return false; }
            OpaaxString    GetFilePath() const override { return OpaaxString(); }
            nlohmann::json GetProperties() const override { return nlohmann::json::object(); }

            void Activate() override {}
            void Deactivate() override {}
            bool IsActive() const override { return false; }

            bool SetConfigValues(OpaaxStringView, const OpaaxString&, TDynArray<OpaaxString>* OutRejected) override
            {
                if (OutRejected != nullptr) { OutRejected->push_back("this app keeps no player's settings"); }
                return false;
            }

            OpaaxString GetConfigValues(OpaaxStringView) const override { return OpaaxString("{}"); }
            void        ResetConfig(OpaaxStringView) override {}
            void        Reset() override {}

            nlohmann::json GetSection(OpaaxStringView) const override { return nullptr; }
            bool           SetSection(OpaaxStringView, const nlohmann::json&) override { return false; }

            TDynArray<DisplayMode> GetDisplayModes() const override { return {}; }
        };

        /** The engine config's name, "Engine": a config is named after its file. */
        const OpaaxString& EngineConfigName()
        {
            static const OpaaxString s_Name = Config::NameFromFile(
                static_cast<const Config*>(Config_Engine::StaticClass().GetDefaultObject())->FileName()).ToString();
            return s_Name;
        }

        /** Sets InValues over the engine config's. False when one is refused: nothing changed. */
        bool SetEngineValues(IUserSettings& InSettings, const nlohmann::json& InValues)
        {
            return InSettings.SetConfigValues(EngineConfigName(), ToJsonText(InValues));
        }
    }

    // =========================================================================
    // Type tag + null object (defined here so there is one of each).
    // =========================================================================
    ServiceTypeID IUserSettings::StaticTypeID() noexcept
    {
        static const int s_Tag = 0;
        return reinterpret_cast<ServiceTypeID>(&s_Tag);
    }

    IUserSettings& IUserSettings::Null()
    {
        static NullUserSettings s_Null;
        return s_Null;
    }

    // =========================================================================
    // IUserSettings
    // =========================================================================
    bool IUserSettings::IsSettingsSlot(const OpaaxStringView InSlot)
    {
        return InSlot.ToString().ToLower() == OpaaxString(SLOT).ToLower();
    }

    bool IUserSettings::SetWindowMode(const EWindowMode InMode)
    {
        nlohmann::json lValues;
        lValues["Window"]["Mode"] = InMode;
        return SetEngineValues(*this, lValues);
    }

    bool IUserSettings::SetResolution(const Uint32 InWidth, const Uint32 InHeight)
    {
        if (InWidth == 0 || InHeight == 0)
        {
            return false;
        }

        nlohmann::json lValues;
        lValues["Window"]["Width"]  = InWidth;
        lValues["Window"]["Height"] = InHeight;
        return SetEngineValues(*this, lValues);
    }

    bool IUserSettings::SetVSync(const bool bInVSync)
    {
        nlohmann::json lValues;
        lValues["Window"]["bVSync"] = bInVSync;
        return SetEngineValues(*this, lValues);
    }

    bool IUserSettings::SetMaxFrameRate(const Uint32 InFramesPerSecond)
    {
        nlohmann::json lValues;
        lValues["Window"]["MaxFrameRate"] = InFramesPerSecond;
        return SetEngineValues(*this, lValues);
    }

    bool IUserSettings::SetMasterVolume(const float InVolume)
    {
        nlohmann::json lValues;
        lValues["Audio"]["MasterVolume"] = Maths::Clamp(InVolume, 0.f, 1.f);
        return SetEngineValues(*this, lValues);
    }

    bool IUserSettings::SetBusVolume(const OpaaxStringID InBus, const float InVolume)
    {
        // The master is not in the list of buses: it has a value of its own.
        if (InBus == AudioMasterBus())
        {
            return SetMasterVolume(InVolume);
        }

        nlohmann::json lValues;
        lValues["Audio"]["Buses"][InBus.CStr()]["Volume"] = Maths::Clamp(InVolume, 0.f, 1.f);
        return SetEngineValues(*this, lValues);
    }

    // =========================================================================
    // UserSettings — CTORS - DTORS
    // =========================================================================
    UserSettings::UserSettings(ISaveStorage& InStorage, IConfigSystem& InConfigs, const IWindowManager& InWindows)
        : m_Storage(&InStorage)
        , m_Configs(&InConfigs)
        , m_Windows(&InWindows)
    {
        m_Configs->OnConfigAdded().AddMember(this, &UserSettings::HandleConfigAdded);
    }

    UserSettings::~UserSettings()
    {
        Unbind();
    }

    void UserSettings::OnShutdown()
    {
        // The config system shuts down after, so it is still whole here; once destroyed, not.
        Unbind();
    }

    // =========================================================================
    // UserSettings — the file
    // =========================================================================
    ESaveResult UserSettings::Load()
    {
        OpaaxString lText;
        ESaveResult lResult = m_Storage->Read(SLOT, lText);

        nlohmann::json lProperties = nlohmann::json::object();
        if (lResult == ESaveResult::Ok)
        {
            SaveGameFile::Contents lContents;
            OpaaxString            lError;
            lResult = SaveGameFile::Parse(lText, lContents, &lError);

            if (lResult == ESaveResult::Ok && lContents.Class != OpaaxStringID(FILE_CLASS))
            {
                lResult = ESaveResult::WrongClass;
                lError  = OpaaxString::Format("it holds a '{}', not the player's settings", lContents.Class.CStr());
            }
            else if (lResult == ESaveResult::Ok && lContents.ClassVersion > FILE_VERSION)
            {
                lResult = ESaveResult::NewerVersion;
                lError  = OpaaxString::Format("written by a newer game (version {}; this one reads up to {})",
                                              lContents.ClassVersion, FILE_VERSION);
            }

            if (lResult == ESaveResult::Ok)
            {
                lProperties = Move(lContents.Properties);
            }
            else
            {
                OPAAX_LOG(LogUserSettings, Warn, "The player's settings were not read ({}): {}. The project's values "
                          "are used ('{}').", ToString(lResult), lError.CStr(), GetFilePath().CStr());
            }
        }
        else if (lResult == ESaveResult::Missing)
        {
            OPAAX_LOG(LogUserSettings, Info, "No player's settings yet: the project's values ('{}')", GetFilePath().CStr());
        }

        // The configs' values are an object of objects; an empty one is the same as none.
        const auto lConfigs = lProperties.find(CONFIGS_SECTION);
        if (lConfigs != lProperties.end() && (!lConfigs->is_object() || lConfigs->empty()))
        {
            if (!lConfigs->is_object())
            {
                OPAAX_LOG(LogUserSettings, Warn, "The player's settings: \"{}\" is not an object, dropped",
                          CONFIGS_SECTION);
            }
            lProperties.erase(lConfigs);
        }

        if (lResult == ESaveResult::Ok)
        {
            OPAAX_LOG(LogUserSettings, Info, "Read the player's settings ('{}')", GetFilePath().CStr());
        }

        m_LoadResult = lResult;
        m_Saved      = lProperties;
        m_Properties = Move(lProperties);

        if (m_bActive)
        {
            ApplyToAll();
        }
        return lResult;
    }

    ESaveResult UserSettings::Save()
    {
        // The slot still holds what the player had, from a newer game or of another class: never written over.
        if (m_LoadResult == ESaveResult::NewerVersion || m_LoadResult == ESaveResult::WrongClass)
        {
            OPAAX_LOG(LogUserSettings, Error, "The player's settings were not saved: '{}' holds a file this game "
                      "does not read ({})", GetFilePath().CStr(), ToString(m_LoadResult));
            return m_LoadResult;
        }

        SaveGameFile::Contents lContents;
        lContents.Class        = OpaaxStringID(FILE_CLASS);
        lContents.ClassVersion = FILE_VERSION;
        lContents.Properties   = m_Properties;

        const ESaveResult lResult = m_Storage->Write(SLOT, SaveGameFile::Serialize(lContents));
        if (lResult == ESaveResult::Ok)
        {
            m_Saved      = m_Properties;
            m_LoadResult = ESaveResult::Ok;
            OPAAX_LOG(LogUserSettings, Info, "Saved the player's settings ('{}')", GetFilePath().CStr());
        }
        return lResult;
    }

    OpaaxString UserSettings::GetFilePath() const
    {
        return m_Storage->GetSlotPath(SLOT);
    }

    // =========================================================================
    // UserSettings — over the configs
    // =========================================================================
    void UserSettings::Activate()
    {
        if (m_bActive || m_Configs == nullptr)
        {
            return;
        }

        m_bActive = true;
        ApplyToAll();
        OPAAX_LOG(LogUserSettings, Trace, "The player's settings are over the configs");
    }

    void UserSettings::Deactivate()
    {
        if (!m_bActive)
        {
            return;
        }

        m_bActive = false;
        ClearConfigs();
        OPAAX_LOG(LogUserSettings, Trace, "The configs read the project's values alone");
    }

    // =========================================================================
    // UserSettings — the configs' values
    // =========================================================================
    bool UserSettings::SetConfigValues(const OpaaxStringView InConfig, const OpaaxString& InValues,
                                       TDynArray<OpaaxString>* OutRejected)
    {
        const auto lRefuse = [OutRejected](OpaaxString InWhy)
        {
            if (OutRejected != nullptr) { OutRejected->push_back(Move(InWhy)); }
            return false;
        };

        Config* const lConfig = (m_Configs != nullptr) ? m_Configs->FindConfigByName(InConfig) : nullptr;
        if (lConfig == nullptr)
        {
            return lRefuse("no config '" + InConfig.ToString() + "'");
        }

        const nlohmann::json lChanges = nlohmann::json::parse(InValues.CStr(), nullptr, false);
        if (lChanges.is_discarded() || !lChanges.is_object())
        {
            return lRefuse("the values are an object shaped like the config's file, like "
                           "{\"Window\": {\"Mode\": \"Fullscreen\"}}");
        }

        const OpaaxString    lName   = lConfig->GetName().ToString();
        const nlohmann::json lValues = ConfigLayer::Merge(FindConfigValues(lName), lChanges);
        const OpaaxString    lText   = ToJsonText(lValues);

        // Checked whole before anything changes: all of them are kept, or none.
        TDynArray<OpaaxString> lRejected;
        if (!lConfig->CheckPlayerText(lText, lRejected))
        {
            return lRefuse("config '" + lName + "' takes no player's values");
        }

        if (!lRejected.empty())
        {
            if (OutRejected != nullptr)
            {
                OutRejected->insert(OutRejected->end(), lRejected.begin(), lRejected.end());
            }
            return false;
        }

        StoreConfigValues(lName, lValues);

        if (m_bActive)
        {
            lConfig->SetPlayerText(lText);
        }
        return true;
    }

    OpaaxString UserSettings::GetConfigValues(const OpaaxStringView InConfig) const
    {
        const nlohmann::json lValues = FindConfigValues(InConfig.ToString());
        return lValues.is_object() ? ToJsonText(lValues, 4) : OpaaxString("{}");
    }

    void UserSettings::ResetConfig(const OpaaxStringView InConfig)
    {
        const OpaaxString lName = InConfig.ToString();
        StoreConfigValues(lName, nlohmann::json::object());

        Config* const lConfig = (m_Configs != nullptr) ? m_Configs->FindConfigByName(InConfig) : nullptr;
        if (m_bActive && lConfig != nullptr && lConfig->HasPlayerValues())
        {
            lConfig->SetPlayerText(OpaaxString("{}"));
        }
    }

    void UserSettings::Reset()
    {
        m_Properties = nlohmann::json::object();

        if (m_bActive)
        {
            ClearConfigs();
        }
    }

    // =========================================================================
    // UserSettings — an engine system's own section
    // =========================================================================
    nlohmann::json UserSettings::GetSection(const OpaaxStringView InName) const
    {
        const auto lSection = m_Properties.find(InName.ToString().CStr());
        return (lSection != m_Properties.end()) ? *lSection : nlohmann::json(nullptr);
    }

    bool UserSettings::SetSection(const OpaaxStringView InName, const nlohmann::json& InValues)
    {
        const OpaaxString lName = InName.ToString();
        if (lName.IsEmpty() || lName == CONFIGS_SECTION)
        {
            return false;
        }

        if (InValues.is_null())
        {
            m_Properties.erase(lName.CStr());
        }
        else
        {
            m_Properties[lName.CStr()] = InValues;
        }
        return true;
    }

    TDynArray<DisplayMode> UserSettings::GetDisplayModes() const
    {
        return m_Windows->GetDisplayModes();
    }

    // =========================================================================
    // UserSettings — internal
    // =========================================================================
    void UserSettings::ApplyToAll()
    {
        // By index: a config reading its change may register another.
        for (Uint64 lIndex = 0; m_Configs != nullptr && lIndex < m_Configs->GetConfigs().size(); ++lIndex)
        {
            ApplyTo(*m_Configs->GetConfigs()[lIndex]);
        }
    }

    void UserSettings::ApplyTo(Config& InConfig)
    {
        const OpaaxString    lName   = InConfig.GetName().ToString();
        const nlohmann::json lValues = FindConfigValues(lName);

        if (lValues.is_null())
        {
            if (InConfig.HasPlayerValues())
            {
                InConfig.SetPlayerText(OpaaxString("{}"));
            }
            return;
        }

        TDynArray<OpaaxString> lRejected;
        if (!lValues.is_object() || !InConfig.SetPlayerText(ToJsonText(lValues), &lRejected))
        {
            OPAAX_LOG(LogUserSettings, Warn, "Dropped the player's values for config '{}': {}", lName.CStr(),
                      lValues.is_object() ? "it takes none" : "they are not an object");
            StoreConfigValues(lName, nlohmann::json::object());
            return;
        }

        for (const OpaaxString& lWhy : lRejected)
        {
            OPAAX_LOG(LogUserSettings, Warn, "Dropped the player's value {}.{}", lName.CStr(), lWhy.CStr());
        }

        // What the config kept: the values it left out are gone from the file at the next save.
        StoreConfigValues(lName, nlohmann::json::parse(InConfig.GetPlayerText().CStr(), nullptr, false));
    }

    void UserSettings::ClearConfigs()
    {
        for (Uint64 lIndex = 0; m_Configs != nullptr && lIndex < m_Configs->GetConfigs().size(); ++lIndex)
        {
            Config& lConfig = *m_Configs->GetConfigs()[lIndex];
            if (lConfig.HasPlayerValues())
            {
                lConfig.SetPlayerText(OpaaxString("{}"));
            }
        }
    }

    nlohmann::json UserSettings::FindConfigValues(const OpaaxString& InName) const
    {
        const auto lConfigs = m_Properties.find(CONFIGS_SECTION);
        if (lConfigs == m_Properties.end() || !lConfigs->is_object())
        {
            return nullptr;
        }

        const auto lValues = lConfigs->find(InName.CStr());
        return (lValues != lConfigs->end()) ? *lValues : nlohmann::json(nullptr);
    }

    void UserSettings::StoreConfigValues(const OpaaxString& InName, const nlohmann::json& InValues)
    {
        nlohmann::json& lConfigs = m_Properties[CONFIGS_SECTION];
        if (!lConfigs.is_object())
        {
            lConfigs = nlohmann::json::object();
        }

        if (InValues.is_object() && !InValues.empty())
        {
            lConfigs[InName.CStr()] = InValues;
        }
        else
        {
            lConfigs.erase(InName.CStr());
        }

        // None left: no section, as a file without values reads.
        if (lConfigs.empty())
        {
            m_Properties.erase(CONFIGS_SECTION);
        }
    }

    void UserSettings::HandleConfigAdded(Config& InConfig)
    {
        if (m_bActive)
        {
            ApplyTo(InConfig);
        }
    }

    void UserSettings::Unbind()
    {
        if (m_Configs != nullptr)
        {
            m_Configs->OnConfigAdded().RemoveAll(this);
            m_Configs = nullptr;
        }
    }
}
