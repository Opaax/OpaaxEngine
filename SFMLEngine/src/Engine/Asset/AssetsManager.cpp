#include "AssetsManager.h"

#include <fstream>
#include <iostream>
#include <sstream>

#include "../ConfigHelper.h"
#include "../EngineConstant.h"

AssetsManager::AssetsManager(const STDString& AssetsConfigPath)
{
    m_configPath = AssetsConfigPath;

    ReadConfig();
}

void AssetsManager::ReadConfig()
{
    if(!m_configPath.empty())
    {
        //Read in the config file.
        std::fstream lConfigInput{m_configPath};

        //Check if the config is valid, else create only a default fonts with defaults.
        if (!lConfigInput.is_open())
        {
            std::cerr << "Failed to open: " << m_configPath << '\n' << "The Asset config path is empty. Will try to load only default fonts" << '\n';
            GatherDefaultFont();
            return;
        }

        GatherDefaultFont();

        STDString lLine;
        while (std::getline(lConfigInput, lLine)) {

            if(ConfigHelper::IsCommentLine(lLine))
            {
                continue;
            }
            
            //Read properties from the config. But Ignore Empty
            std::istringstream lLineStream(lLine);
            std::string lPropName{};
            
            if (!(lLineStream >> lPropName))
            {
                continue;
            }

            //Windows properties
            if(lPropName == AllAssetType[EAsset_Font])
            {
                STDString lFontName;
                STDString lFontPath;
                
                lLineStream >> lFontName;
                lLineStream >> lFontPath;

                sf::Font lFont;

                if(!lFontName.empty() && lFont.openFromFile(lFontPath))
                {
                    m_fonts[lFontName] = lFont;
                }else
                {
                    std::cerr << "New font not valid name or path -- Name: " << lFontName << " -- Path: " << "lFontPath";
                }
            }
        }
    }

    GatherDefaultFont();
}

void AssetsManager::GatherDefaultFont()
{
    //will be exception if this path is not valid.
    sf::Font lDefaultFont{Engine::FONT_DEFAULT};
    m_fonts["Default"] = lDefaultFont;
}
