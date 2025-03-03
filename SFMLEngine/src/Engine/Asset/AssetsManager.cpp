#include "AssetsManager.h"

#include "../EngineConstant.h"
#include "SimpleIni.h"

AssetsManager::AssetsManager(const STDString& AssetsConfigPath)
{
    m_configPath = AssetsConfigPath;
    m_config = MakeUnique<ConfigIni>(m_configPath);

    ReadConfig();
}

void AssetsManager::SetupDefaultConfig() const
{
    if(m_config->IsJustCreated())
    {
        m_config->SetString(AllAssetType[EAsset_Font], Engine::FONT_DEFAULT_NAME, Engine::FONT_DEFAULT_PATH);
        m_config->SetString(AllAssetType[EAsset_Texture], Engine::TEXTURE_DEFAULT_3232_NAME, Engine::TEXTURE_DEFAULT_3232_PATH);
        m_config->SetString(AllAssetType[EAsset_Texture], Engine::TEXTURE_DEFAULT_6464_NAME, Engine::TEXTURE_DEFAULT_6464_PATH);
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
    lSectionsNames.sort(CSimpleIniA::Entry::LoadOrder());
    
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
                    m_fonts[lKey.pItem] = MakeUnique<sf::Font>(lFont);
                }
            }
        }

        else if(lSection.pItem == AllAssetType[EAsset_Texture])
        {
            for (auto& lKey : lSectionsKeys)
            {
                sf::Texture lTexture;
                STDString lPath = m_config->GetString(AllAssetType[EAsset_Texture], lKey.pItem);
                if(lTexture.loadFromFile(lPath))
                {
                    m_texture[lKey.pItem] = MakeUnique<sf::Texture>(lTexture);
                }
            }
        }

        else if(lSection.pItem == AllAssetType[EAsset_SpriteSheet])
        {
            for (auto& lKey : lSectionsKeys)
            {
                CSimpleIni::TNamesDepend lOutValue;
                m_config->GetIni().GetAllValues(AllAssetType[EAsset_SpriteSheet].c_str(), lKey.pItem, lOutValue);

                STDString lTextureName;
                UInt8 lSize;
                UInt8 lCount;
                int lIndex = 0;
                for (auto lVal : lOutValue)
                {
                    switch (lIndex)
                    {
                    case 0:
                        lTextureName = lVal.pItem;
                        break;
                    case 1:
                        lSize = static_cast<UInt8>(std::stoi(lVal.pItem));
                        break;
                    case 2:
                        lCount = static_cast<UInt8>(std::stoi(lVal.pItem));
                        break;
                    }
                    
                    lIndex++;
                }
                
                sf::Texture lTexture{*m_texture[lTextureName].get()};
                m_spriteSheet[lKey.pItem] = MakeUnique<SpriteSheet>(lTexture,lSize, lCount);
            }
        }
    }
}