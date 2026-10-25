#pragma once

#include <nlohmann/json.hpp>

#include "Core/IO/FileIO.h"
#include "Core/OpaaxMacro.hpp"
#include "Core/Reflection/OpaaxProperty.h"
#include "Core/Serialization/JsonConcept.h"
#include "Core/String/OpaaxString.hpp"
#include "Engine/Config/Config.h"
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

    // =============================================================================
    // TConfig — a config whose values are a TData. A concrete config derives from it, declares
    //   OPAAX_OBJECT(Class, Config) and its file name; the base handles Load, Save, the default file and
    //   what the editor draws (TData's OPAAX_PROPERTIES).
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
        const TData& GetData() const { return m_Data; }
        TData&       GetData()       { return m_Data; }

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
                return FileIO::WriteAllText(InAbsPath, TConfigCodec<TData>::ToText(m_Data));
            }

            // Bad JSON or wrong-typed value: keep the defaults and return false
            // (ConfigSystem logs the warning).
            try
            {
                m_Data = TConfigCodec<TData>::FromText(lText);
            }
            catch (const nlohmann::json::exception&)
            {
                m_Data = TData{};
                NotifyChanged();
                return false;
            }

            NotifyChanged();
            return true;
        }

        bool Save(const OpaaxString& InAbsPath) override
        {
            m_LoadedPath = InAbsPath;
            return FileIO::WriteAllText(InAbsPath, TConfigCodec<TData>::ToText(m_Data));
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
        OpaaxString ToText() const override { return TConfigCodec<TData>::ToText(m_Data); }

        bool VisitValues(IPropertyVisitor& InVisitor) override
        {
            if constexpr (CReflected<TData>)
            {
                VisitProperties(m_Data, InVisitor);
                return true;
            }
            else
            {
                return false;
            }
        }

        void ResetToDefaults() override { m_Data = TData{}; }
        //~End Config interface

        // =============================================================================
        // Members
        // =============================================================================
    protected:
        TData       m_Data;       // in-memory defaults until Load
        OpaaxString m_LoadedPath;
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
