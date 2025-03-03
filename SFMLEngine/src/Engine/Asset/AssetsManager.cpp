#include "AssetsManager.h"

#include "../EngineConstant.h"
#include "SimpleIni.h"

AssetsManager::AssetsManager(const STDString& AssetsConfigPath)
{
    m_configPath = AssetsConfigPath;

    m_config = MakeUnique<ConfigIni>("config/AssetsConfig.ini");

    ReadConfig();
}

void AssetsManager::SetupDefaultConfig() const
{
    if(m_config->IsJustCreated())
    {
        m_config->SetString(AllAssetType[EAsset_Font], "Default", Engine::FONT_DEFAULT);
        m_config->Save();
    }
}

void AssetsManager::ReadConfig()
{
    if(!m_config || !m_config->IsReadable())
    {
        return;
    }

    SetupDefaultConfig();

    CSimpleIni::TNamesDepend lSectionsNames;
    m_config->GetAllSection(lSectionsNames);
    
    for (auto& lSection : lSectionsNames)
    {
        CSimpleIni::TNamesDepend lSectionsKeys;
        m_config->GetAllKeysInSection(lSection.pItem, lSectionsKeys);
        
        if(lSection.pItem == AllAssetType[EAsset_Font])
        {
            for (auto& lKey : lSectionsKeys)
            {
                sf::Font lFont;
                STDString lPath = m_config->GetString(AllAssetType[EAsset_Font], lKey.pItem);
                if(lFont.openFromFile(lPath))
                {
                    m_fonts[lKey.pItem] = lFont;
                }
            }
        }
    }
}