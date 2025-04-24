#pragma once
#include "Opaax/OpaaxCoreMacros.h"

//Keep here for simplicity as all imgui context these necessary
#include <imgui.h>
#include <imgui_impl_sdl3.h>

namespace OPAAX
{
    namespace IMGUI
    {
        class OPAAX_API OpaaxImguiBase
        {
        public:
            OpaaxImguiBase() = default;
            virtual ~OpaaxImguiBase() = default;

            virtual void Shutdown() = 0;
        };
    }
}

