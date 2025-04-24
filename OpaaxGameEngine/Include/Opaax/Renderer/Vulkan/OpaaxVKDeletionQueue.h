#pragma once
#include <ranges>

#include "OpaaxVulkanInclude.h"
#include "Opaax/OpaaxStdTypes.h"

namespace OPAAX
{
    namespace RENDERER
    {
        namespace VULKAN
        {
            struct OPAAX_API OpaaxVKDeletionQueue
            {
                std::deque<OPSTDFunc<void()>> Deletors;

                void PushFunction(std::function<void()>&& Function)
                {
                    Deletors.push_back(Function);
                }

                void Flush()
                {
                    // reverse iterate the deletion queue to execute all the functions
                    for (auto& Deletor : std::ranges::reverse_view(Deletors))
                    {
                        Deletor(); //call functors
                    }

                    Deletors.clear();
                }
            };
        }
    }
}
