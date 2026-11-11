#pragma once

// =============================================================================
// BuildInfo — what kind of build this is, from the definitions CMake gives the engine.
//   A dev build reads its content from the source tree and has the developer tools (profiler,
//   console, cheats); a ship build is what players run.
// =============================================================================

#if !defined(OPAAX_DEV_BUILD)
    #error "OPAAX_DEV_BUILD is defined by Engine/CMakeLists.txt: link OpaaxEngine to get it."
#endif

namespace Opaax::Build
{
    /** True in a dev build (the debug-editor and debug presets), false in a ship build (release). */
    constexpr bool IsDevBuild() noexcept
    {
        return OPAAX_DEV_BUILD != 0;
    }
}
