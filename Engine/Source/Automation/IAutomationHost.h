#pragma once

#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"

namespace Opaax
{
    // =============================================================================
    // IAutomationHost — what the engine's commands ask of the app hosting them (the application).
    // =============================================================================
    /** Receives a frame's pixels (IAutomationHost::RequestFrameRead). */
    using FrameReader = TFunction<void(const TDynArray<Uint8>& InRGBA, Uint32 InWidth, Uint32 InHeight)>;

    class IAutomationHost
    {
    public:
        virtual ~IAutomationHost() = default;

        /** Saves the frame being drawn as a PNG at InPath once it is drawn. */
        virtual void RequestScreenshot(const OpaaxString& InPath) = 0;

        /**
         * Gives InReader the frame being drawn, once it is drawn: RGBA pixels, rows from top to
         * bottom, and its size. With no frame to read, the pixels are empty and the size 0.
         * One reader per frame: a second request replaces the first.
         */
        virtual void RequestFrameRead(FrameReader InReader) = 0;

        /** Closes the app at the end of this frame. */
        virtual void RequestQuit() = 0;

        /** Frames since the app started. */
        virtual Uint64 GetFrameIndex() const = 0;
    };
}
