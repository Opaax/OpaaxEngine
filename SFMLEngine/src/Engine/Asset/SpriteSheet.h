#pragma once
#include <SFML/Graphics/Texture.hpp>

#include "../EngineType.hpp"

class SpriteSheet
{
    const sf::Texture& m_texture{};
    UInt8 m_imageSize{};
    UInt8 m_imageCount{};
public:
    SpriteSheet() = default;
    SpriteSheet(const sf::Texture& InTexture, UInt8 InImgSize, UInt8 InImgCount):
    m_texture{InTexture},
    m_imageSize{InImgSize},
    m_imageCount{InImgCount}
    {}

    UInt8 GetSize() const {return m_imageSize;}
    UInt8 GetImgCount() const {return m_imageCount;}
};
