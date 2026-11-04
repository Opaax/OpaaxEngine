#pragma once

#include <nlohmann/json.hpp>

#include "Core/IO/FileIO.h"
#include "Core/OpaaxMacro.hpp"
#include "Core/Reflection/OpaaxProperty.h"
#include "Core/Serialization/JsonConcept.h"
#include "Core/String/OpaaxString.hpp"
#include "Core/String/OpaaxStringJson.h"
#include "Engine/Config/Config.h"
#include "Engine/Config/ConfigLayer.h"
#include "Engine/Reflection/PropertyVisitor.h"

// =============================================================================
// ==== USAGE ==================================================================
// =============================================================================
// A config data type uses the same nlohmann macro as a component.
//
// In your .h:
//   #include "Engine/Config/TConfig.h"
//   struct MyConfigData
//   {
//       float Value = 10.f;
//
//       NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(MyConfigData, Value)
//       OPAAX_PROPERTIES(MyConfigData, OPAAX_PROP(Value))                  // what the Config panel draws
//   };
//
//   DECLARE_T_CONFIG(MyConfig, MyConfigData)                               // Config_MyConfig, MyConfig.config
//
// In your .cpp:
//   OPAAX_REGISTER_OBJECT(Config_MyConfig);
//
// Read it with IConfigSystem::Get<Config_MyConfig>().GetData(). Nested structs are written as nested
// JSON objects and shown as groups in the editor.
// =============================================================================
// ==== END USAGE ==============================================================
// =============================================================================

namespace Opaax
{
    // =============================================================================
    // TConfigCodec — (de)serialization of a config data type.
    //   Works for any type with the nlohmann macro. Specialize it for a custom format.
    //   Throws on bad input; TConfig::Load catches it and keeps the defaults.
    // =============================================================================
    template<class TData>
    struct TConfigCodec
    {
        static_assert(CJsonSerializable<TData>,
                      "A config data type needs NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT "
                      "(or its own TConfigCodec specialization).");

        static TData FromText(const OpaaxString& InText)
        {
            return nlohmann::json::parse(InText.CStr()).template get<TData>();
        }

        static OpaaxString ToText(const TData& InData)
        {
            // dump(4), sorted keys
            return OpaaxString(nlohmann::json(InData).dump(4).c_str());
        }
    };

    namespace ConfigMerge
    {
        /**
         * The first key of InPatch that InValues does not have at the same place.
         * @param OutPath Its dotted path ("Time.FixRate")
         * @param OutKnown The keys InValues has there ("FixedRate")
         * @return False when every key of InPatch is known
         */
        inline bool FindUnknownKey(const nlohmann::json& InValues, const nlohmann::json& InPatch, OpaaxString& OutPath,
                                   OpaaxString& OutKnown, const OpaaxString& InPrefix = OpaaxString())
        {
            if (!InPatch.is_object() || !InValues.is_object())
            {
                return false;
            }

            for (const auto& lEntry : InPatch.items())
            {
                const OpaaxString lPath = InPrefix.IsEmpty() ? OpaaxString(lEntry.key().c_str())
                                                             : InPrefix + "." + lEntry.key().c_str();
                const auto        lKnown = InValues.find(lEntry.key());
                if (lKnown == InValues.end())
                {
                    OutPath  = lPath;
                    OutKnown = OpaaxString();
                    for (const auto& lHas : InValues.items())
                    {
                        OutKnown += OutKnown.IsEmpty() ? OpaaxString(lHas.key().c_str()) : ", " + OpaaxString(lHas.key().c_str());
                    }
                    return true;
                }

                if (FindUnknownKey(*lKnown, lEntry.value(), OutPath, OutKnown, lPath))
                {
                    return true;
                }
            }
            return false;
        }
    }

    // =============================================================================
    // TConfig — a config whose values are a TData. A concrete config derives from it, declares
    //   OPAAX_OBJECT(Class, Config) and its file name; the base handles Load, Save, the default file,
    //   what the editor draws (TData's OPAAX_PROPERTIES) and the player's values over the project's.
    // =============================================================================
    template<class TData>
    class TConfig : public Config
    {
        // =============================================================================
        // Function
        // =============================================================================
    public:
        using DataType = TData;

        // ==== Getter =========================================================================
        /** What the engine reads: the project's values with the player's over them. */
        const TData& GetData() const { return m_Data; }

        /** The project's values: the file's, with what the Config panel and MergeText changed. */
        const TData& GetProjectData() const { return m_Project; }

        // =============================================================================
        // Override
        // =============================================================================

        //~Begin Config interface
    public:
        bool Load(const OpaaxString& InAbsPath) override
        {
            m_LoadedPath = InAbsPath;

            const OpaaxString lText = FileIO::ReadAllText(InAbsPath);
            if (lText.IsEmpty())
            {
                // Missing or empty: write the default file, keep the defaults.
                return FileIO::WriteAllText(InAbsPath, TConfigCodec<TData>::ToText(m_Project));
            }

            // Bad JSON or wrong-typed value: keep the defaults and return false
            // (ConfigSystem logs the warning).
            try
            {
                m_Project = TConfigCodec<TData>::FromText(lText);
            }
            catch (const nlohmann::json::exception&)
            {
                m_Project = TData{};
                NotifyChanged();
                return false;
            }

            NotifyChanged();
            return true;
        }

        bool Save(const OpaaxString& InAbsPath) override
        {
            m_LoadedPath = InAbsPath;
            return FileIO::WriteAllText(InAbsPath, TConfigCodec<TData>::ToText(m_Project));
        }

        bool Save() override
        {
            if (m_LoadedPath.IsEmpty())
            {
                return false;
            }
            return Save(m_LoadedPath);
        }

        // Same codec as Save.
        OpaaxString ToText() const override { return TConfigCodec<TData>::ToText(m_Project); }

        OpaaxString ToEffectiveText() const override { return TConfigCodec<TData>::ToText(m_Data); }

        // Through the codec's text, so a value it would refuse in the file is refused here.
        bool MergeText(const OpaaxString& InValues, OpaaxString& OutError) override
        {
            try
            {
                nlohmann::json       lValues = nlohmann::json::parse(ToText().CStr());
                const nlohmann::json lPatch  = nlohmann::json::parse(InValues.CStr());

                if (!lPatch.is_object())
                {
                    OutError = "the values are an object shaped like the file, like {\"Time\": {\"FixedRate\": 30}}";
                    return false;
                }

                OpaaxString lUnknown;
                OpaaxString lKnown;
                if (ConfigMerge::FindUnknownKey(lValues, lPatch, lUnknown, lKnown))
                {
                    OutError = "no value '" + lUnknown + "' in this config; there: " + lKnown;
                    return false;
                }

                lValues.merge_patch(lPatch);
                m_Project = TConfigCodec<TData>::FromText(ToJsonText(lValues));
            }
            catch (const nlohmann::json::exception& InError)
            {
                OutError = InError.what();
                return false;
            }

            NotifyChanged();
            return true;
        }

        bool VisitValues(IPropertyVisitor& InVisitor) override
        {
            if constexpr (CReflected<TData>)
            {
                VisitProperties(m_Project, InVisitor);
                return true;
            }
            else
            {
                return false;
            }
        }

        void ResetToDefaults() override
        {
            m_Project = TData{};
            RefreshValues();
        }

        bool SetPlayerText(const OpaaxString& InValues, TDynArray<OpaaxString>* OutRejected = nullptr) override
        {
            const nlohmann::json lLayer = nlohmann::json::parse(InValues.CStr(), nullptr, false);
            if (lLayer.is_discarded() || !lLayer.is_object())
            {
                return false;
            }

            const OpaaxString lBefore = ToEffectiveText();
            ApplyPlayer(lLayer, OutRejected);

            if (ToEffectiveText() != lBefore)
            {
                Announce();
            }
            return true;
        }

        OpaaxString GetPlayerText() const override { return ToJsonText(m_Player, 4); }

        bool HasPlayerValues() const override { return !m_Player.empty(); }

    protected:
        // The project's values may have dropped what a player's value was for: such a value goes.
        void RefreshValues() override
        {
            TDynArray<OpaaxString> lDropped;
            ApplyPlayer(m_Player, &lDropped);

            for (const OpaaxString& lWhy : lDropped)
            {
                OPAAX_LOG(LogConfig, Warn, "Config [{}]: dropped a player's value, {}", FileName(), lWhy.CStr());
            }
        }
        //~End Config interface

        // =============================================================================
        // Internal
        // =============================================================================
    private:
        /**
         * Keeps the values of InLayer that fit the project's, each checked by the codec on its own, and
         * reads the project's values with them over.
         */
        void ApplyPlayer(const nlohmann::json& InLayer, TDynArray<OpaaxString>* OutRejected)
        {
            const nlohmann::json lProject = nlohmann::json::parse(TConfigCodec<TData>::ToText(m_Project).CStr());

            nlohmann::json lAccepted = nlohmann::json::object();
            for (const ConfigLayer::Leaf& lLeaf : ConfigLayer::Leaves(InLayer))
            {
                nlohmann::json lValues = lProject;
                OpaaxString    lWhy;
                if (!ConfigLayer::Apply(lValues, lLeaf.Layer))
                {
                    lWhy = "no such value";
                }
                else
                {
                    try
                    {
                        (void)TConfigCodec<TData>::FromText(ToJsonText(lValues));
                    }
                    catch (const nlohmann::json::exception&)
                    {
                        lWhy = "a value of the wrong type";
                    }
                }

                if (!lWhy.IsEmpty())
                {
                    if (OutRejected != nullptr) { OutRejected->push_back(lLeaf.Path + ": " + lWhy); }
                    continue;
                }

                lAccepted.merge_patch(lLeaf.Layer);
            }

            nlohmann::json lValues = lProject;
            ConfigLayer::Apply(lValues, lAccepted);
            try
            {
                m_Data = TConfigCodec<TData>::FromText(ToJsonText(lValues));
            }
            catch (const nlohmann::json::exception&)
            {
                // Each value read on its own, but not all together: the project's values alone.
                m_Data    = m_Project;
                lAccepted = nlohmann::json::object();
                if (OutRejected != nullptr) { OutRejected->push_back("the player's values do not read together"); }
            }
            m_Player = Move(lAccepted);
        }

        // =============================================================================
        // Members
        // =============================================================================
    protected:
        TData          m_Project;                                  // in-memory defaults until Load
        TData          m_Data;                                     // m_Project with m_Player over it
        nlohmann::json m_Player = nlohmann::json::object();        // the player's values, shaped like the file
        OpaaxString    m_LoadedPath;
    };
}

// =============================================================================
// Declares Config_<ConfigName>, saved as <ConfigName>.config, its values a DataType. Register it in a
// .cpp with OPAAX_REGISTER_OBJECT(Config_<ConfigName>).
// =============================================================================
#define DECLARE_T_CONFIG(ConfigName, DataType)                                                      \
    class Config_##ConfigName final : public ::Opaax::TConfig<DataType>                            \
    {                                                                                               \
        OPAAX_OBJECT(Config_##ConfigName, ::Opaax::Config)                                          \
    public:                                                                                         \
        const char* FileName() const override { return STR(ConfigName) ".config"; }               \
    };
