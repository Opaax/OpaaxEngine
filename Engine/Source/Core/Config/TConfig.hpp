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
// A config data type serializes EXACTLY as a component does — one macro, no codec, no key
// constants, no hand-written parser (that was two idioms for one job).
//
// In your .h:
//   #include "Core/Config/TConfig.hpp"
//   struct MyConfigData
//   {
//       float Value = 10.f;
//
//       NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(MyConfigData, Value)   // _WITH_DEFAULT: see IConfig.h
//       OPAAX_PROPERTIES(MyConfigData, OPAAX_PROP(Value))                  // optional — the editor draws it
//   };
//
//   DECLARE_OPAAX_T_CONFIG(MyConfig, MyConfigData)   // exported; DECLARE_T_CONFIG if DLL-internal
//
// In your .cpp:
//   IMPL_T_CONFIG(MyConfig)
//
// Nest structs to nest the file: a member whose type is another such struct writes as a json
// object, and the editor draws it as a group.
// =============================================================================
// ==== END USAGE ==============================================================
// =============================================================================

namespace Opaax
{
    // =============================================================================
    // TConfigCodec — the (de)serialization contract for a config data type.
    //
    //   The default IS the answer now: any data type carrying the nlohmann macro serializes with no
    //   codec of its own, exactly as a component does. It used to be an undefined primary that every
    //   data type specialized by hand, which meant a config was ~200 lines of defensive parsing
    //   while a component was one line — two idioms for one job.
    //
    //   "You forgot" is still a compile error, just a better one: the static_assert below names the
    //   macro instead of the linker naming a missing specialization. A type that genuinely needs a
    //   bespoke format can still specialize this template.
    //
    //   IT THROWS ON BAD INPUT, deliberately — TConfig::Load catches, keeps the in-memory defaults
    //   and answers false, so tolerance lives in ONE place rather than in every parser.
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
            // dump(4) with nlohmann's sorted keys — the byte shape every .config already has.
            return OpaaxString(nlohmann::json(InData).dump(4).c_str());
        }
    };

    // =============================================================================
    // TConfig — generic config base: a concrete config DECLARES its data type and
    // file name, the base owns all the mechanical plumbing (read/parse/store on Load,
    // serialize/write on Save, default-file generation on a miss). 
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
        /***/
        const TData& GetData() const { return m_Data; }
        /***/
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
                // Missing (or empty) — write the default file, keep the in-memory defaults.
                return GenerateDefaultConfig(InAbsPath);
            }

            // THE ONE TOLERANT READER. Every config used to hand-write this — try/catch around the
            // parse, contains() + is_string() per field — and the macro does none of it. Absent keys
            // are already covered (_WITH_DEFAULT), so what lands here is malformed json or a
            // wrong-typed value: keep the defaults and say so. Core does not log (I11); the caller
            // (ConfigSystem) turns the false into a Warn naming the file.
            try
            {
                m_Data = TConfigCodec<TData>::FromText(lText);
            }
            catch (const nlohmann::json::exception&)
            {
                m_Data = TData{};
                return false;
            }

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

        // The codec Save writes through — so what a reader is shown is the file that would be written,
        // never a second rendering of the same data.
        OpaaxString ToText() const override { return TConfigCodec<TData>::ToText(m_Data); }

    protected:
        bool GenerateDefaultConfig(const OpaaxString& InAbsPath) override
        {
            // The default template is the serialized in-memory defaults.
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

// NOTE: DECLARE_T_CONFIG_CODEC and DECLARE_CONFIG_DATA are GONE. They existed to bind a data type
// to a hand-written Parse/Serialize pair; TConfigCodec's default does that job for every type
// carrying the nlohmann macro, which is the same one a component carries.