#pragma once

#include <nlohmann/json.hpp>

#include "IConfig.h"
#include "Core/IO/FileIO.h"
#include "Core/Serialization/JsonConcept.h"
#include "Core/String/OpaaxString.hpp"
#include "Core/OpaaxMacro.hpp"
#include "Core/EngineAPI.h"

// =============================================================================
// ==== USAGE ==================================================================
// =============================================================================
// A config data type uses the same nlohmann macro as a component.
//
// In your .h:
//   #include "Core/Config/TConfig.hpp"
//   struct MyConfigData
//   {
//       float Value = 10.f;
//
//       NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(MyConfigData, Value)
//       OPAAX_PROPERTIES(MyConfigData, OPAAX_PROP(Value))                  // optional: editor display
//   };
//
//   DECLARE_OPAAX_T_CONFIG(MyConfig, MyConfigData)   // in the engine DLL
//   DECLARE_T_CONFIG(MyConfig, MyConfigData)         // anywhere else
//
// In your .cpp:
//   IMPL_T_CONFIG(MyConfig)
//
// Nested structs are written as nested JSON objects and shown as groups in the editor.
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
    // TConfig — generic config base. A concrete config declares its data type and
    // file name; the base handles Load, Save and the default file.
    // =============================================================================
    template<class TData>
    class TConfig : public IConfig
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

        //~Begin Opaax::IConfig interface
    public:
        bool Load(const OpaaxString& InAbsPath) override
        {
            m_LoadedPath = InAbsPath;

            const OpaaxString lText = FileIO::ReadAllText(InAbsPath);
            if (lText.IsEmpty())
            {
                // Missing or empty: write the default file, keep the defaults.
                return GenerateDefaultConfig(InAbsPath);
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

    protected:
        bool GenerateDefaultConfig(const OpaaxString& InAbsPath) override
        {
            // The default file is the serialized defaults.
            return FileIO::WriteAllText(InAbsPath, TConfigCodec<TData>::ToText(m_Data));
        }
        //~End Opaax::IConfig interface

        // =============================================================================
        // Members
        // =============================================================================
    protected:
        TData       m_Data;       // in-memory defaults until Load
        OpaaxString m_LoadedPath;
    };
}
    
// =============================================================================
// Which macro depends on the module that declares the config:
//   DECLARE_OPAAX_T_CONFIG — in the engine DLL (exported).
//   DECLARE_T_CONFIG       — in any other module (editor lib, game module). Not exported;
//                            using OPAAX_API there gives C4273 then LNK2019.
// =============================================================================
#define DECLARE_OPAAX_T_CONFIG(ConfigName, DataType)\
class OPAAX_API Config_##ConfigName final : public TConfig<DataType> \
{ public: OPAAX_CONFIG_TYPE(ConfigName) const char* FileName() const override { return STR(ConfigName) ".config"; } };

#define DECLARE_T_CONFIG(ConfigName, DataType)\
class Config_##ConfigName final : public TConfig<DataType> \
{ public: OPAAX_CONFIG_TYPE(ConfigName) const char* FileName() const override { return STR(ConfigName) ".config"; } };

#define IMPL_T_CONFIG(ConfigName)\
::Opaax::ConfigTypeID Config_##ConfigName::StaticTypeID() noexcept\
{\
    static const int s_Tag = 0;\
    return reinterpret_cast<::Opaax::ConfigTypeID>(&s_Tag);\
}