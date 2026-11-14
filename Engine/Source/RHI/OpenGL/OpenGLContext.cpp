#include "RHI/OpenGL/OpenGLContext.h"

#include <glad/glad.h>

#include "Core/Log/Logger.h"
#include "Core/String/CString.h"

namespace Opaax
{
    namespace
    {
        /** True when the current context lists the extension InName. */
        bool HasExtension(const char* InName)
        {
            GLint lCount = 0;
            glGetIntegerv(GL_NUM_EXTENSIONS, &lCount);
            for (GLint lIndex = 0; lIndex < lCount; ++lIndex)
            {
                const GLubyte* lExtension = glGetStringi(GL_EXTENSIONS, static_cast<GLuint>(lIndex));
                if (lExtension != nullptr && CString::Equals(reinterpret_cast<const char*>(lExtension), InName))
                {
                    return true;
                }
            }
            return false;
        }
    }

    // =============================================================================
    // OpenGLContext
    // =============================================================================
    OpenGLContext::OpenGLContext(IGLSurface& InSurface)
        : m_Surface(&InSurface)
    {
    }

    bool OpenGLContext::Init()
    {
        m_Surface->MakeCurrent();

        const GLProcLoader lLoader = m_Surface->GetProcLoader();
        if (lLoader == nullptr || !gladLoadGLLoader(lLoader))
        {
            OPAAX_LOG(LogOpenGLContext, Error, "Failed to initialize GLAD");
            return false;
        }

        // glad loads by version, and debug output is core from 4.3 only. A 4.1 context has the same
        // functions under the same names when the driver lists GL_KHR_debug.
        if (!glad_glDebugMessageCallback && HasExtension("GL_KHR_debug"))
        {
            glad_glDebugMessageCallback = reinterpret_cast<PFNGLDEBUGMESSAGECALLBACKPROC>(lLoader("glDebugMessageCallback"));
            glad_glDebugMessageControl  = reinterpret_cast<PFNGLDEBUGMESSAGECONTROLPROC>(lLoader("glDebugMessageControl"));
        }

        // VSync on until the renderer applies Window.bVSync.
        SetSwapInterval(1);

        LogAdapterInfo();
        return true;
    }

    void OpenGLContext::LogAdapterInfo() const
    {
        auto lStr = [](GLenum InName) -> const char*
        {
            const GLubyte* lValue = glGetString(InName);
            return lValue ? reinterpret_cast<const char*>(lValue) : "<unknown>";
        };

        GLint lMaxTexUnits = 0, lMaxTexSize = 0;
        glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &lMaxTexUnits);
        glGetIntegerv(GL_MAX_TEXTURE_SIZE,        &lMaxTexSize);
        
        // One line with the GPU and driver info.
        OPAAX_LOG(LogOpenGLContext, Info, "OpenGL {} on {} ({}) — GLSL {}, {} texture units, max texture {}",
                  lStr(GL_VERSION), lStr(GL_RENDERER), lStr(GL_VENDOR), lStr(GL_SHADING_LANGUAGE_VERSION),
                  lMaxTexUnits, lMaxTexSize);
    }

    void OpenGLContext::SwapBuffers()
    {
        m_Surface->SwapBuffers();
    }

    void OpenGLContext::SetSwapInterval(const Uint32 InRefreshes)
    {
        m_Surface->SetSwapInterval(InRefreshes);
    }

} // namespace Opaax
