// Suite: the UI designer's layout aspect (Editor/UI/UIPreviewAspect.h). A named aspect is exact
// (16:9 shows what a 1920x1080 game draws); Free uses the panel's own size.
#include <doctest.h>

#include "Core/String/OpaaxString.hpp"
#include "Editor/UI/UIPreviewAspect.h"

#include <iterator>   // std::size

using namespace Opaax;
using namespace Opaax::Editor;

TEST_CASE("UIPreviewAspect: a named aspect lays out at exactly that ratio; Free is the framebuffer's size")
{
    CHECK(PreviewLayoutSize(EUIPreviewAspect::Free, 777u, 333u) == Vector2u32{ 777u, 333u });

    const Vector2u32 lWide = PreviewLayoutSize(EUIPreviewAspect::W16x9, 777u, 333u);
    CHECK(lWide == Vector2u32{ 1920u, 1080u });
    CHECK(static_cast<float>(lWide.x) / static_cast<float>(lWide.y) == doctest::Approx(16.f / 9.f));

    CHECK(PreviewLayoutSize(EUIPreviewAspect::W21x9,  1u, 1u) == Vector2u32{ 2520u, 1080u });
    CHECK(PreviewLayoutSize(EUIPreviewAspect::W4x3,   1u, 1u) == Vector2u32{ 480u,  360u });
    CHECK(PreviewLayoutSize(EUIPreviewAspect::W16x10, 1u, 1u) == Vector2u32{ 1920u, 1200u });
    CHECK(PreviewLayoutSize(EUIPreviewAspect::P9x16,  1u, 1u) == Vector2u32{ 1080u, 1920u });

    // Every entry of the combo's list has a name, and the list holds every enumerator once.
    CHECK(std::size(kUIPreviewAspects) == 6u);
    for (const EUIPreviewAspect lAspect : kUIPreviewAspects)
    {
        CHECK(ToString(lAspect) != nullptr);
    }
    CHECK(OpaaxString(ToString(EUIPreviewAspect::W21x9)) == OpaaxString("21:9"));
}
