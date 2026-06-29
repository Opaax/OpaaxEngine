#pragma once

#include "IConfig.h"
#include "ConfigIO.h"
#include "Core/OpaaxString.hpp"
#include "Core/OpaaxMacro.hpp"
#include "Core/EngineAPI.h"

// =============================================================================
// ==== USAGE ==================================================================
// =============================================================================
// In you .h file
//#include "Core/Config/TConfig.hpp"
//using namespace Opaax;
//struct MyConfigData
//{
//  public:
//        float m_value = 10;
//    You can either make static Parse / Serialize
//    static MyConfigData Parse(const OpaaxString& InJsonText) { return MyConfigData(); }
//    static OpaaxString Serialize(const MyConfigData& InData) { return OpaaxString(); }
//    or use DECLARE_CONFIG_DATA(DataType) that create both func above
//};
// OR either inline global
//  inline MyConfigData ParseMyConfigData(const OpaaxString& InJsonText) { return MyConfigData(); }
//  inline OpaaxString SerializeMyConfigData(const MyConfigData& InData) { return OpaaxString(); }
//
// Make the codec struct that IConfig use to load/save
//DECLARE_T_CONFIG_CODEC(MyConfigData::Parse, MyConfigData::Serialize, MyConfigData)
//or
//DECLARE_T_CONFIG_CODEC(ParseMyConfigData, SerializeMyConfigData, MyConfigData)
//
// Make the Config type it self
//DECLARE_T_CONFIG(MyConfig, MyConfigData)
//
// In you .cpp file
//
//#include "your.h"
//
// IMPL_T_CONFIG(MyConfig)
//
// =============================================================================
// ==== END USAGE ==============================================================
// =============================================================================

namespace Opaax
{
    // =============================================================================
    // TConfigCodec — the (de)serialization contract for a config data type. Each
    // data type provides a specialization (declared next to the data, forwarding to
    // its pure parse/serialize functions). The undefined primary makes "you forgot to
    // specialize" a clear compile error rather than a silent fallback.
    // =============================================================================
    template<class TData>
    struct TConfigCodec;

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
        const TData& Data() const { return m_Data; }
        /***/
        TData&       Data()       { return m_Data; }

        // =============================================================================
        // Override
        // =============================================================================

        //~Begin Opaax::IConfig interface
    public:
        bool Load(const OpaaxString& InAbsPath) override
        {
            m_LoadedPath = InAbsPath;

            const OpaaxString lText = ConfigIO::ReadText(InAbsPath);
            if (lText.IsEmpty())
            {
                // Missing (or empty) — write the default file, keep the in-memory defaults.
                return GenerateDefaultConfig(InAbsPath);
            }

            m_Data = TConfigCodec<TData>::FromText(lText);
            return true;
        }

        bool Save(const OpaaxString& InAbsPath) override
        {
            m_LoadedPath = InAbsPath;
            return ConfigIO::WriteText(InAbsPath, TConfigCodec<TData>::ToText(m_Data));
        }

        bool Save() override
        {
            if (m_LoadedPath.IsEmpty())
            {
                return false;
            }
            return Save(m_LoadedPath);
        }

    protected:
        bool GenerateDefaultConfig(const OpaaxString& InAbsPath) override
        {
            // The default template is the serialized in-memory defaults.
            return ConfigIO::WriteText(InAbsPath, TConfigCodec<TData>::ToText(m_Data));
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

#define DECLARE_T_CONFIG_CODEC(ParseFunc, SerializeFunc, DataType)\
template<class T> struct ::Opaax::TConfigCodec;\
template<> struct ::Opaax::TConfigCodec<DataType>\
{\
static  DataType                    FromText(const ::Opaax::OpaaxString& InText)        { return ParseFunc(InText); }\
static ::Opaax::OpaaxString         ToText(const DataType& InData)                      { return SerializeFunc(InData); }\
};

#define DECLARE_CONFIG_DATA(DataType)\
    static DataType Parse(const ::Opaax::OpaaxString& InJsonText);\
    static ::Opaax::OpaaxString Serialize(const DataType& InData);