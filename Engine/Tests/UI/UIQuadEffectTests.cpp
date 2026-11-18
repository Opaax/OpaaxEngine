// Suite: quad effects (UIShadowModifier, UIOutlineModifier). They copy a widget's quads after it
// rebuilds, under the originals: the copies come first in the list. Each copy keeps its quad's texture,
// UVs and cut, takes the effect's colour, and its alpha is the effect's times the quad's.
#include <doctest.h>

#include "UI/Modifiers/UIOutlineModifier.h"
#include "UI/Modifiers/UIShadowModifier.h"
#include "UI/UICanvas.h"
#include "UI/Widgets/UIImage.h"

using namespace Opaax;

namespace
{
    struct Fixture
    {
        UICanvas Canvas{ 1080.f };
        UIImage* Image = nullptr;

        Fixture()
        {
            Canvas.SetTargetSize(1920, 1080);
            Image = static_cast<UIImage*>(Canvas.Root().AddChild(NewObject<UIImage>()));
            UIRect lRect;
            lRect.SizeDelta = { 100.f, 40.f };
            Image->SetRect(lRect);
            Image->Brush.Tint = LinearColor(1.f, 0.5f, 0.25f, 0.8f);
            Canvas.Update();
        }

        const TDynArray<UIQuad>& Quads()
        {
            Canvas.Update();
            return Image->GetQuads();
        }
    };

    void CheckVec(const Vector2F& InA, const Vector2F& InB)
    {
        CHECK(InA.x == doctest::Approx(InB.x));
        CHECK(InA.y == doctest::Approx(InB.y));
    }
}

TEST_CASE("UIShadowModifier: doubles the quads, its copies first, moved by Offset")
{
    Fixture lF;
    REQUIRE(lF.Quads().size() == 1u);
    const UIQuad lOriginal = lF.Quads()[0];

    lF.Image->AddModifier<UIShadowModifier>().SetOffset({ 6.f, -3.f });
    const TDynArray<UIQuad>& lQuads = lF.Quads();
    REQUIRE(lQuads.size() == 2u);

    // The shadow draws under: it comes first.
    CheckVec(lQuads[0].Bounds.Center, { lOriginal.Bounds.Center.x + 6.f, lOriginal.Bounds.Center.y - 3.f });
    CheckVec(lQuads[0].Bounds.Size(), lOriginal.Bounds.Size());
    CheckVec(lQuads[1].Bounds.Center, lOriginal.Bounds.Center);
    CHECK(lQuads[1].Color.x == doctest::Approx(1.f));
}

TEST_CASE("UIShadowModifier: its colour, its alpha times each quad's")
{
    Fixture lF;
    lF.Image->AddModifier<UIShadowModifier>().SetColor(LinearColor(0.1f, 0.2f, 0.3f, 0.5f));
    const UIQuad& lShadow = lF.Quads()[0];

    CHECK(lShadow.Color.x == doctest::Approx(0.1f));
    CHECK(lShadow.Color.y == doctest::Approx(0.2f));
    CHECK(lShadow.Color.z == doctest::Approx(0.3f));
    CHECK(lShadow.Color.w == doctest::Approx(0.5f * 0.8f));
}

TEST_CASE("UIOutlineModifier: four copies first, Distance left, right, up and down")
{
    Fixture lF;
    const Vector2F lCentre = lF.Quads()[0].Bounds.Center;

    lF.Image->AddModifier<UIOutlineModifier>().SetDistance(3.f);
    const TDynArray<UIQuad>& lQuads = lF.Quads();
    REQUIRE(lQuads.size() == 5u);
    CheckVec(lQuads[0].Bounds.Center, { lCentre.x - 3.f, lCentre.y });
    CheckVec(lQuads[1].Bounds.Center, { lCentre.x + 3.f, lCentre.y });
    CheckVec(lQuads[2].Bounds.Center, { lCentre.x, lCentre.y + 3.f });
    CheckVec(lQuads[3].Bounds.Center, { lCentre.x, lCentre.y - 3.f });
    CheckVec(lQuads[4].Bounds.Center, lCentre);
    CHECK(lQuads[0].Color.w == doctest::Approx(0.8f));   // black, alpha 1 times the image's 0.8

    // At 0 the rim is nothing: no copies.
    static_cast<UIOutlineModifier*>(lF.Image->GetModifiers()[0].Get())->SetDistance(0.f);
    CHECK(lF.Quads().size() == 1u);
}

TEST_CASE("Quad effects: a cut fill's shadow is cut too, and effects stack in list order")
{
    Fixture lF;
    lF.Image->Fill       = EUIFill::Horizontal;
    lF.Image->FillAmount = 0.5f;
    lF.Image->InvalidateContent();
    REQUIRE(lF.Quads().size() == 1u);
    const UIQuad lCut = lF.Quads()[0];
    CHECK(lCut.Bounds.Size().x == doctest::Approx(50.f));

    lF.Image->AddModifier<UIShadowModifier>();
    CHECK(lF.Quads()[0].Bounds.Size().x == doctest::Approx(50.f));
    CHECK(lF.Quads()[0].UVMax.x == doctest::Approx(lCut.UVMax.x));

    // An outline after the shadow rims both: (1 + 1) x 5.
    lF.Image->AddModifier<UIOutlineModifier>();
    CHECK(lF.Quads().size() == 10u);

    // Disabled, removed: back to the shadow alone, then nothing added.
    lF.Image->GetModifiers()[1].Get()->bEnabled = false;
    lF.Image->InvalidateContent();
    CHECK(lF.Quads().size() == 2u);
    lF.Image->RemoveModifier(*lF.Image->GetModifiers()[0]);
    CHECK(lF.Quads().size() == 1u);
}

TEST_CASE("Quad effects: a field edit rebuilds the quads, nothing else does")
{
    Fixture lF;
    UIShadowModifier& lShadow = lF.Image->AddModifier<UIShadowModifier>();
    lF.Quads();

    // An idle frame rebuilds nothing.
    CHECK(lF.Canvas.Update().Rebuilds == 0u);

    lShadow.Offset = { 10.f, 0.f };   // the editor writes the field, then says so
    lShadow.PostEditChange();
    CHECK(lF.Canvas.Update().Rebuilds == 1u);
    CHECK(lF.Image->GetQuads()[0].Bounds.Center.x == doctest::Approx(10.f));
}
