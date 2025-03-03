#include "ConfigIni.h"

#include <filesystem>
#include <fstream>

ConfigIni::ConfigIni(const STDString& IniPath):m_iniPath{IniPath}
{
    CheckNIfCreate();
    
    m_ini = MakeUnique<CSimpleIni>(true, true, true);
    m_ini->SetUnicode();
    m_ini->SetMultiLine(true);

    switch (m_ini->LoadFile(m_iniPath.c_str()))
    {
    case SI_FILE:
        //File error (see errno for detail error)
    case SI_NOMEM:
        //Out of memory error
    case SI_FAIL:
        //Generic failure
        bCanBeRead = false;
        std::cerr << "Config ini path:" << m_iniPath << " cannot be open" << std::endl;
        break;
    case SI_OK:
        //No Error
    case SI_INSERTED:
        bCanBeRead = true;
        std::cout << "Config ini path:" << m_iniPath << " opened" << std::endl;
        break;
    default:
        break;
    }
}

void ConfigIni::CheckNIfCreate()
{
    if(std::filesystem::exists(m_iniPath))
    {
        bIsJustCreated = false;
        return;
    }
    
    std::ofstream lNewFile;
    
    lNewFile.open (m_iniPath, std::fstream::app);
    lNewFile.close();

    bIsJustCreated = true;
}

STDString ConfigIni::GetString(const STDString& InSection, const STDString& InKey, const STDString& InDefaultValue) const
{
    if(!m_ini)
    {
        return "Invalid";
    }
    
    return m_ini->GetValue(InSection.c_str(), InKey.c_str(), InDefaultValue.c_str());
}

int ConfigIni::GetInt(const STDString& InSection, const STDString& InKey, int InDefaultValue) const
{
    if(!m_ini)
    {
        return InDefaultValue;
    }
    
    return static_cast<int>(m_ini->GetLongValue(InSection.c_str(), InKey.c_str(), InDefaultValue));
}

bool ConfigIni::GetBool(const STDString& InSection, const STDString& InKey, bool InDefaultValue) const
{
    if(!m_ini)
    {
        return InDefaultValue;
    }
    
    return m_ini->GetBoolValue(InSection.c_str(), InKey.c_str(), InDefaultValue);
}

void ConfigIni::GetAllSection(CSimpleIni::TNamesDepend& OutSectionsNames) const
{
    if(!m_ini)
    {
        return;
    }
    
    m_ini->GetAllSections(OutSectionsNames);
}

void ConfigIni::GetAllKeysInSection(const STDString& InSectionName, CSimpleIniA::TNamesDepend& OutKeys) const
{
    if(!m_ini)
    {
        return;
    }
    
    m_ini->GetAllKeys(InSectionName.c_str(), OutKeys);
}

void ConfigIni::SetString(const STDString& InSection, const STDString& InKey, const STDString& Value) const
{
    if(!m_ini)
    {
        return;
    }
    
    m_ini->SetValue(InSection.c_str(), InKey.c_str(), Value.c_str());
}

void ConfigIni::SetInt(const STDString& InSection, const STDString& InKey, int value) const
{
    if(!m_ini)
    {
        return;
    }
    
    m_ini->SetLongValue(InSection.c_str(), InKey.c_str(), value);
}

void ConfigIni::SetBool(const STDString& InSection, const STDString& InKey, bool value) const
{
    if(!m_ini)
    {
        return;
    }
    
    m_ini->SetBoolValue(InSection.c_str(), InKey.c_str(), value);
}

void ConfigIni::Save() const
{
    if(!m_ini)
    {
        return;
    }

    switch (m_ini->SaveFile(m_iniPath.c_str()))
    {
    case SI_FILE:
        //File error (see errno for detail error)
    case SI_NOMEM:
        //Out of memory error
    case SI_FAIL:
        //Generic failure
        std::cerr << "Config ini path:" << m_iniPath << " cannot be saved" << std::endl;
        break;
    case SI_OK:
        //No Error
    case SI_INSERTED:
        std::cout << "Config ini path:" << m_iniPath << " saved" << std::endl;
        break;
    default:
        break;
    }
}
