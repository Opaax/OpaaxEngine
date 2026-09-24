// Suite: IRenderTarget implementations (Renderer/RenderTarget.hpp).
//
// OffscreenRenderTarget is the D2 output contract — a NON-OWNING IRenderTarget that forwards
// Bind/Unbind + size to a wrapped IFramebuffer, so the editor's ViewportPanel can render the world
// into a texture. Both it and DefaultRenderTarget are header-inline, so this suite compiles them
// directly — no GL, no DLL render symbol. A StubFramebuffer test double stands in for the concrete
// OpenGLFramebuffer: it records Bind/Unbind calls and reports a settable size, letting us pin the
// forwarding contract without a GPU context.
#include <doctest.h>

#include "Renderer/RenderTarget.hpp"
#include "RHI/Framebuffer.h"

using namespace Opaax;

namespace
{
    // Minimal IFramebuffer double — no GL. Counts binds and reports a settable size, so the tests
    // can prove the wrapper reads THROUGH rather than caching.
    class StubFramebuffer final : public IFramebuffer
    {
    public:
        void Bind()   override { ++BindCount;   }
        void Unbind() override { ++UnbindCount; }
        void Resize(Uint32 InWidth, Uint32 InHeight) override { Width = InWidth; Height = InHeight; }

        Uint32 GetColorAttachmentID() const noexcept override { return ColorID; }
        Uint32 GetWidth()             const noexcept override { return Width;   }
        Uint32 GetHeight()            const noexcept override { return Height;  }

        Uint32 Width       = 320;
        Uint32 Height      = 240;
        Uint32 ColorID     = 42;
        int    BindCount   = 0;
        int    UnbindCount = 0;
    };
}

TEST_CASE("OffscreenRenderTarget: size reads through to the wrapped framebuffer")
{
    StubFramebuffer       lFb;
    OffscreenRenderTarget lTarget(&lFb);

    CHECK(lTarget.GetWidth()  == 320u);
    CHECK(lTarget.GetHeight() == 240u);

    // A resize of the underlying FBO is reflected immediately — the target caches nothing (D2: the
    // target's size is the single source of truth for the view each frame).
    lFb.Resize(800, 600);
    CHECK(lTarget.GetWidth()  == 800u);
    CHECK(lTarget.GetHeight() == 600u);
}

TEST_CASE("OffscreenRenderTarget: Bind/Unbind forward to the framebuffer")
{
    StubFramebuffer       lFb;
    OffscreenRenderTarget lTarget(&lFb);

    lTarget.Bind();
    lTarget.Bind();
    lTarget.Unbind();

    CHECK(lFb.BindCount   == 2);
    CHECK(lFb.UnbindCount == 1);
}

TEST_CASE("OffscreenRenderTarget: GetFramebuffer exposes the wrapped pointer (command-buffer dispatch)")
{
    // A command-buffer backend dispatches on this: non-null -> render into the FBO image, never present.
    StubFramebuffer       lFb;
    OffscreenRenderTarget lTarget(&lFb);
    CHECK(lTarget.GetFramebuffer() == &lFb);
}

TEST_CASE("OffscreenRenderTarget: a null framebuffer is inert, never dereferenced")
{
    OffscreenRenderTarget lTarget(nullptr);

    CHECK(lTarget.GetFramebuffer() == nullptr);
    CHECK(lTarget.GetWidth()  == 0u);
    CHECK(lTarget.GetHeight() == 0u);
    lTarget.Bind();   // must not crash on a null wrapped pointer
    lTarget.Unbind();
}

TEST_CASE("DefaultRenderTarget: backbuffer path reports a null framebuffer (D4 unchanged)")
{
    DefaultRenderTarget lTarget(1280, 720);

    CHECK(lTarget.GetFramebuffer() == nullptr);   // null -> present surface, not an FBO
    CHECK(lTarget.GetWidth()  == 1280u);
    CHECK(lTarget.GetHeight() == 720u);

    lTarget.OnResize(1920, 1080);
    CHECK(lTarget.GetWidth()  == 1920u);
    CHECK(lTarget.GetHeight() == 1080u);

    lTarget.Bind();   // no-op — the backbuffer is the default framebuffer
    lTarget.Unbind();
}
