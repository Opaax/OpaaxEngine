#include "RHI/OpenGL/OpenGLDebug.h"

#include "Core/Log/Logger.h"

#include <glad/glad.h>

namespace Opaax
{
    OPAAX_LOG_CATEGORY(OpenGL);
}

namespace Opaax::OpenGLDebug
{
    namespace
    {
        SeenMessages g_Seen;

        /** Says once that the list of different messages is full. */
        void LogWhenFull()
        {
            if (g_Seen.IsFull())
            {
                OPAAX_LOG(LogOpenGL, Warn, "{} different OpenGL messages were logged: later new ones are not.",
                          SeenMessages::CAPACITY);
            }
        }

        void APIENTRY OnDriverMessage(const GLenum InSource, const GLenum InType, const GLuint InId,
                                      const GLenum InSeverity, const GLsizei /*InLength*/,
                                      const GLchar* InMessage, const void* /*InUser*/)
        {
            const EReport lReport = Classify(InType, InSeverity);

            if (lReport == EReport::Ignore)
            {
                return;
            }

            // The same message from the same source is the same fault.
            const Uint64 lKey = (static_cast<Uint64>(InSource) << 48) ^ (static_cast<Uint64>(InType) << 32) ^ InId;

            if (!g_Seen.AddIsFirst(lKey))
            {
                return;
            }

            const char* lText = InMessage ? InMessage : "<no text>";

            if (lReport == EReport::Error)
            {
                OPAAX_LOG(LogOpenGL, Error, "{} ({}, id {}): {}", TypeName(InType), SourceName(InSource), InId, lText);
            }
            else
            {
                OPAAX_LOG(LogOpenGL, Warn, "{} ({}, id {}): {}", TypeName(InType), SourceName(InSource), InId, lText);
            }

            LogWhenFull();
        }
    }

    bool Install()
    {
        // Loaded by the context: core from OpenGL 4.3, the GL_KHR_debug extension below.
        if (!glad_glDebugMessageCallback || !glad_glDebugMessageControl)
        {
            return false;
        }

        GLint lFlags = 0;
        glGetIntegerv(GL_CONTEXT_FLAGS, &lFlags);

        if ((lFlags & GL_CONTEXT_FLAG_DEBUG_BIT) == 0)
        {
            return false;
        }

        // Synchronous: the message comes from inside the wrong call, so a breakpoint in the
        // callback shows who made it.
        glEnable(GL_DEBUG_OUTPUT);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        glDebugMessageCallback(OnDriverMessage, nullptr);
        glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);

        return true;
    }

    void CheckErrors()
    {
        // A lost context answers an error forever: stop after a few.
        for (Uint32 i = 0; i < 16; ++i)
        {
            const GLenum lError = glGetError();

            if (lError == GL_NO_ERROR)
            {
                return;
            }

            if (g_Seen.AddIsFirst(lError))
            {
                OPAAX_LOG(LogOpenGL, Error, "{} raised during the frame (this driver does not say by which call).",
                          ErrorName(lError));
                LogWhenFull();
            }
        }
    }
}
