#pragma once
#include "Opaax/Renderer/IOpaaxRendererContext.h"

namespace OPAAX
{
    namespace Vulkan
    {
        class OPAAX_API OpaaxVulkanRenderer final : public OPAAX::IOpaaxRendererContext
        {
        public:
            explicit OpaaxVulkanRenderer(OPAAX::OpaaxWindow* const Window)
                : IOpaaxRendererContext(Window) {}

            ~OpaaxVulkanRenderer() override;
            
            bool Initialize() override;
            void Resize() override;
            void RenderFrame() override;
            void Shutdown() override;
            FORCEINLINE SDL_WindowFlags GetWindowFlags() override { return (SDL_WindowFlags)(SDL_WINDOW_VULKAN); }
        };
    }
}
