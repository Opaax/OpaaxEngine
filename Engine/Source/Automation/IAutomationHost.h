#pragma once

#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"

namespace Opaax
{
    // =============================================================================
    // IAutomationHost — what the engine's commands ask of the app hosting them (the application).
    // =============================================================================
    class IAutomationHost
    {
    public:
        virtual ~IAutomationHost() = default;

        /** Saves the frame being drawn as a PNG at InPath once it is drawn. */
        virtual void RequestScreenshot(const OpaaxString& InPath) = 0;

        /** Closes the app at the end of this frame. */
        virtual void RequestQuit() = 0;

        /** Frames since the app started. */
        virtual Uint64 GetFrameIndex() const = 0;
    };
}
