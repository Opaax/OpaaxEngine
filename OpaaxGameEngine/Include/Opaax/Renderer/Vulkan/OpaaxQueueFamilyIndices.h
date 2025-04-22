#pragma once

#include "Opaax/OpaaxStdTypes.h"

namespace OPAAX
{
    namespace RENDERER
    {
        namespace VULKAN
        {
            struct OPAAX_API OpaaxQueueFamilyIndices
            {
                TOptional<UInt32> GraphicsFamily;
                TOptional<UInt32> PresentFamily;

                bool IsComplete()
                {
                    return GraphicsFamily.has_value() && PresentFamily.has_value();
                }
            };
        }
    }
}
