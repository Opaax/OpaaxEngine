#pragma once
#include <SFML/Graphics/Color.hpp>

#include "../Math/Math.hpp"

struct SFMLExtension
{
    static sf::Color GetRandomColor()
    {
        return sf::Color{static_cast<uint8_t>(Math::RandomRange(0, 255)),static_cast<uint8_t>(Math::RandomRange(0, 255)),static_cast<uint8_t>(Math::RandomRange(0, 255))};
    }
};
