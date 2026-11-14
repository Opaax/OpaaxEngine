// Suite: the graphics context and its window's surface — what an OpenGL context asks of the window
// before it is made, and that the context goes through the surface for everything the window does.
// A fake surface: no window, no GL context needed.
#include <doctest.h>

#include "Core/Build/BuildInfo.h"
#include "RHI/IGraphicsContext.h"
#include "RHI/OpenGL/OpenGLContext.h"

using namespace Opaax;

namespace
{
    // A window's surface that counts what it is asked.
    class FakeGLSurface final : public IGLSurface
    {
    public:
        Int32  MadeCurrent = 0;
        Int32  Swaps       = 0;
        Uint32 Interval    = 99;

        void         MakeCurrent() override { ++MadeCurrent; }
        GLProcLoader GetProcLoader() const override { return nullptr; }
        void         SwapBuffers() override { ++Swaps; }
        void         SetSwapInterval(const Uint32 InRefreshes) override { Interval = InRefreshes; }
    };
}

TEST_CASE("GraphicsContext: OpenGL asks its window for 4.1 core, forward compatible; a debug context in a dev build only")
{
    const GLSurfaceDesc lDesc = IGraphicsContext::DescribeSurface(EBackend::OpenGL);
    CHECK(lDesc.MajorVersion == 4u);
    CHECK(lDesc.MinorVersion == 1u);
    CHECK(lDesc.bCoreProfile);
    CHECK(lDesc.bForwardCompatible);
    CHECK(lDesc.bDebugContext == Build::IsDevBuild());
}

TEST_CASE("GraphicsContext: the OpenGL context swaps and sets the interval through its surface; no loader, no context")
{
    FakeGLSurface lSurface;
    TUniquePtr<IGraphicsContext> lContext = IGraphicsContext::Create(EBackend::OpenGL, lSurface);
    REQUIRE(lContext);

    lContext->SetSwapInterval(2);
    CHECK(lSurface.Interval == 2u);
    lContext->SwapBuffers();
    lContext->SwapBuffers();
    CHECK(lSurface.Swaps == 2);

    // A surface that cannot find GL functions: Init makes it current, then fails (logged) without a call to GL.
    CHECK_FALSE(lContext->Init());
    CHECK(lSurface.MadeCurrent == 1);
}
