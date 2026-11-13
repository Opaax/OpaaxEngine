#pragma once

#include "Core/OpaaxTypes.h"

// =============================================================================
// OpenGLDebug — what the driver says is wrong, written to the log.
//   With a debug context (OpenGL 4.3, or GL_KHR_debug on an older one) the driver reports each
//   wrong call with its own text, as it happens. Without one (macOS) the error flags are read once
//   per frame.
//   Dev builds only: a ship build asks for neither.
//   The functions that only sort and name values take plain numbers, so they are tested without
//   a GL context.
// =============================================================================
namespace Opaax::OpenGLDebug
{
    /** How a driver message is logged. */
    enum class EReport : Uint8
    {
        Ignore,
        Warn,
        Error
    };

    /**
     * Sorts a driver message by its GL type and severity (the GL_DEBUG_TYPE_* and
     * GL_DEBUG_SEVERITY_* values).
     * @return Error for a wrong call; Warn for undefined, deprecated, non-portable or slow use the
     *         driver rates medium or high; Ignore for the rest (notes, markers, low severity)
     */
    constexpr EReport Classify(const Uint32 InType, const Uint32 InSeverity) noexcept
    {
        constexpr Uint32 lTypeError    = 0x824C;
        constexpr Uint32 lTypeFirst    = 0x824D;   // deprecated behaviour
        constexpr Uint32 lTypeLast     = 0x8250;   // performance
        constexpr Uint32 lSeverityHigh = 0x9146;
        constexpr Uint32 lSeverityMid  = 0x9147;

        if (InType == lTypeError)
        {
            return EReport::Error;
        }

        const bool bRated = InSeverity == lSeverityHigh || InSeverity == lSeverityMid;
        const bool bUsage = InType >= lTypeFirst && InType <= lTypeLast;

        return (bRated && bUsage) ? EReport::Warn : EReport::Ignore;
    }

    /** The name of a glGetError value ("GL_INVALID_OPERATION"), or "unknown error". */
    constexpr const char* ErrorName(const Uint32 InError) noexcept
    {
        switch (InError)
        {
            case 0x0500: return "GL_INVALID_ENUM";
            case 0x0501: return "GL_INVALID_VALUE";
            case 0x0502: return "GL_INVALID_OPERATION";
            case 0x0505: return "GL_OUT_OF_MEMORY";
            case 0x0506: return "GL_INVALID_FRAMEBUFFER_OPERATION";
            default:     return "unknown error";
        }
    }

    /** Who sent a driver message (a GL_DEBUG_SOURCE_* value), in a word. */
    constexpr const char* SourceName(const Uint32 InSource) noexcept
    {
        switch (InSource)
        {
            case 0x8246: return "API";
            case 0x8247: return "window system";
            case 0x8248: return "shader compiler";
            case 0x8249: return "third party";
            case 0x824A: return "application";
            default:     return "other";
        }
    }

    /** What a driver message is about (a GL_DEBUG_TYPE_* value), in a word or two. */
    constexpr const char* TypeName(const Uint32 InType) noexcept
    {
        switch (InType)
        {
            case 0x824C: return "error";
            case 0x824D: return "deprecated";
            case 0x824E: return "undefined behaviour";
            case 0x824F: return "portability";
            case 0x8250: return "performance";
            default:     return "other";
        }
    }

    /**
     * Remembers which messages were already logged: a wrong call made every frame is one line in
     * the log, not one per frame.
     */
    class SeenMessages
    {
    public:
        static constexpr Uint32 CAPACITY = 64;

        /**
         * @param InKey What makes two messages the same one
         * @return True the first time a key is given. False for a repeat, and for every new key
         *         once CAPACITY different ones were seen.
         */
        bool AddIsFirst(const Uint64 InKey) noexcept
        {
            for (Uint32 i = 0; i < m_Count; ++i)
            {
                if (m_Keys[i] == InKey)
                {
                    return false;
                }
            }

            if (m_Count == CAPACITY)
            {
                return false;
            }

            m_Keys[m_Count++] = InKey;
            return true;
        }

        /** True once CAPACITY different messages were seen: later new ones are not logged. */
        bool IsFull() const noexcept { return m_Count == CAPACITY; }

    private:
        Uint64 m_Keys[CAPACITY] = {};
        Uint32 m_Count          = 0;
    };

    /**
     * Asks the driver to report wrong calls to the log. Call once, with the context current.
     * @return False when the context has no debug output (the driver has none, or it is not a debug
     *         context): CheckErrors is then the way
     */
    bool Install();

    /**
     * Logs the error flags raised since the last call, each kind once. For contexts where
     * Install returned false; cheap enough for once a frame, not for every call.
     */
    void CheckErrors();
}
