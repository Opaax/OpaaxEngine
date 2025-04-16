#pragma once
#include <optional>

#include "OpaaxTypes.h"

struct OpaaxQueueFamilyIndices
{
    std::optional<UInt32> GraphicsFamily;
    std::optional<UInt32> PresentFamily;

    bool IsComplete()
    {
        return GraphicsFamily.has_value() && PresentFamily.has_value();
    }
};
