#pragma once
#include <map>
#include <string>
#include <vector>
#include <SFML/Graphics/Font.hpp>

#include "../EngineType.hpp"

/**
 * Types of assets.
 */
enum EAssetsType
{
    Asset_InvalidType= -1,
#define ACTION_TYPE_AS_ENUM 1
#include "AssetEnumType.h"
#undef ACTION_TYPE_AS_ENUM
    Asset_Count,
};

template<typename ValueType>
using AssetMap = std::map<STDString, ValueType>;

class AssetsManager
{
private:
    STDString m_configPath;
    
    AssetMap<sf::Font> m_fonts;

private:
    void ReadConfig();
    void GatherDefaultFont();
    
public:
    /**
* const strings for assets types.
* Use at init state to parse asset config file.
* Runtime jobs should use Enum type.
* However, we can gather name by giving the enum type as param e.g. GAllAssetType[EAssetsType::Type];
*/
    std::vector<STDString> AllAssetType{
#define ACTION_TYPE_AS_STRING 1
#include "AssetEnumType.h"
#undef ACTION_TYPE_AS_STRING
    };
public:
    AssetsManager() = default;
    AssetsManager(const STDString& AssetsConfigPath);

    const AssetMap<sf::Font>& GetFonts() const {return m_fonts;}
    sf::Font& GetFont(const STDString& FontName) {return m_fonts[FontName];}
};
