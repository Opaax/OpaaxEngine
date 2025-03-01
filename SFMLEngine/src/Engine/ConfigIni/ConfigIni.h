#pragma once
#include <iostream>
#include <SimpleIni.h>
#include "../EngineType.hpp"

class ConfigIni
{
private:
    TUniquePtr<CSimpleIni> m_ini;
    STDString m_iniPath;

    ConfigIni() = default;
public:
    /**
     * 
     * @param IniPath 
     */
    explicit ConfigIni(const STDString& IniPath);

    /**
     * 
     * @param InSection
     * @param InKey
     * @param InDefaultValue
     * @return 
     */
    STDString GetString(const STDString& InSection, const STDString& InKey, const STDString& InDefaultValue = "") const;

    /**
     * 
     * @param InSection 
     * @param InKey 
     * @param InDefaultValue
     * @return 
     */
    int GetInt(const STDString& InSection, const STDString& InKey, int InDefaultValue = 0) const;

    /**
     * 
     * @param InSection 
     * @param InKey 
     * @param InDefaultValue 
     * @return 
     */
    bool GetBool(const STDString& InSection, const STDString& InKey, bool InDefaultValue = false) const;

    /**
     * 
     * @param InSection 
     * @param InKey 
     * @param Value 
     */
    void SetString(const STDString& InSection, const STDString& InKey, const STDString& Value) const;

    /**
     * 
     * @param InSection 
     * @param InKey 
     * @param value 
     */
    void SetInt(const STDString& InSection, const STDString& InKey, int value) const;

    /**
     * 
     * @param InSection 
     * @param InKey 
     * @param value 
     */
    void SetBool(const STDString& InSection, const STDString& InKey, bool value) const;

    /**
     * 
     */
    void Save() const;
};
