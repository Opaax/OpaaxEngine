// Suite: the UI mask (UI/Widgets/UIMask.h, UICanvas::BuildDrawList) — UI16 / UI17.
//
// The draw LIST is the seam that makes this testable at all: which mask applies to which quad is
// decided by a tree walk that Submit would otherwise bury behind a GL context. Everything here is
// about that pairing — the pixels (white shows, black hides) are the shader's half and are the
// one thing only their eyes can confirm.
#include <doctest.h>

#include "RHI/Texture.h"
#include "UI/UICanvas.h"
#include "UI/Widgets/UIImage.h"
#include "UI/Widgets/UIMask.h"
#include "UI/Widgets/UIPanel.h"
#include "UI/Widgets/UIText.h"

using namespace Opaax;

namespace
{
    /** Never sampled — only its identity matters here. */
    class FakeTexture final : public ITexture2D
    {
    public:
        void   Bind(Uint32) const override {}
        void   Unbind()     const override {}
        Uint32 GetWidth()      const noexcept override { return 8u; }
        Uint32 GetHeight()     const noexcept override { return 8u; }
        Uint32 GetRendererID() const noexcept override { return 2u; }
        bool   IsLoaded()      const noexcept override { return true; }
    };

    class StubAssets final : public IUIAssetProvider
    {
    public:
        FakeTexture Texture;
        bool        bUploaded = true;
        Uint32      Resolves  = 0;
        OpaaxString LastPath;

        FontFaceView ResolveFace(const char*) override { return FontFaceView{}; }

        ITexture2D* ResolveTexture(const char* InAssetPath) override
        {
            ++Resolves;
            LastPath = InAssetPath;
            return bUploaded ? &Texture : nullptr;
        }
    };

    UIRect Centred(const Vector2F& InSize)
    {
        UIRect lRect;
        lRect.SizeDelta = InSize;
        return lRect;
    }

    /** An image always emits exactly one quad, which makes the draw list easy to reason about. */
    UIImage* AddImage(UIWidget& InParent, const char* InName)
    {
        auto lImage  = MakeUnique<UIImage>();
        lImage->Name = InName;
        lImage->SetRect(Centred({ 50.f, 50.f }));
        return static_cast<UIImage*>(InParent.AddChild(Move(lImage)));
    }

    /** The item whose quad belongs to InWidget, or a null item. */
    UIDrawItem ItemFor(const TDynArray<UIDrawItem>& InItems, const UIWidget& InWidget)
    {
        for (const UIDrawItem& lItem : InItems)
        {
            if (!InWidget.GetQuads().empty() && lItem.Quad == &InWidget.GetQuads()[0]) { return lItem; }
        }
        return UIDrawItem{};
    }

    struct Fixture
    {
        UICanvas   Canvas{ 1080.f };
        StubAssets Assets;

        Fixture() { Canvas.SetTargetSize(1920, 1080); }

        void Update() { Canvas.Update(UIBuildContext{ &Assets }); }

        TDynArray<UIDrawItem> DrawList()
        {
            Update();
            TDynArray<UIDrawItem> lItems;
            Canvas.BuildDrawList(lItems);
            return lItems;
        }
    };
}

TEST_CASE("UIMask: everything UNDER a mask carries it; a sibling outside carries none")
{
    Fixture lF;

    auto lMaskNode  = MakeUnique<UIMask>();
    lMaskNode->Name = "Mask";
    lMaskNode->SetRect(Centred({ 200.f, 200.f }));
    UIMask* const lMask = static_cast<UIMask*>(lF.Canvas.Root().AddChild(Move(lMaskNode)));

    UIImage* const lInside = AddImage(*lMask, "Inside");
    UIImage* const lOutside = AddImage(lF.Canvas.Root(), "Outside");

    // A masked TEXT is the case that costs nothing extra — text goes through UIQuad like the rest.
    auto lTextNode  = MakeUnique<UIText>();
    lTextNode->Name = "Label";
    UIWidget* const lText = lMask->AddChild(Move(lTextNode));

    const TDynArray<UIDrawItem> lItems = lF.DrawList();

    CHECK(ItemFor(lItems, *lInside).Mask == lMask);
    CHECK(ItemFor(lItems, *lOutside).Mask == nullptr);
    CHECK(lText->GetQuads().empty());   // no face resolved, so it emitted nothing — and did not crash

    // The mask itself draws NOTHING, so it contributes no item of its own.
    CHECK(lMask->GetQuads().empty());
}

TEST_CASE("UIMask: a nested mask REPLACES its ancestor — nearest wins")
{
    Fixture lF;

    auto lOuterNode = MakeUnique<UIMask>();
    lOuterNode->SetRect(Centred({ 400.f, 400.f }));
    UIMask* const lOuter = static_cast<UIMask*>(lF.Canvas.Root().AddChild(Move(lOuterNode)));

    UIImage* const lUnderOuter = AddImage(*lOuter, "UnderOuter");

    auto lInnerNode = MakeUnique<UIMask>();
    lInnerNode->SetRect(Centred({ 100.f, 100.f }));
    UIMask* const lInner = static_cast<UIMask*>(lOuter->AddChild(Move(lInnerNode)));

    UIImage* const lUnderInner = AddImage(*lInner, "UnderInner");

    const TDynArray<UIDrawItem> lItems = lF.DrawList();

    CHECK(ItemFor(lItems, *lUnderOuter).Mask == lOuter);
    CHECK(ItemFor(lItems, *lUnderInner).Mask == lInner);   // the NEAREST, not the outer
}

TEST_CASE("UIMask: the mask's RECT is its own resolved bounds, so moving it moves the cut-out")
{
    Fixture lF;

    auto lMaskNode = MakeUnique<UIMask>();
    lMaskNode->SetRect(Centred({ 200.f, 100.f }));
    UIMask* const lMask = static_cast<UIMask*>(lF.Canvas.Root().AddChild(Move(lMaskNode)));
    AddImage(*lMask, "Inside");

    lF.Update();
    CHECK(lMask->GetBounds().Size().x == doctest::Approx(200.f));

    UIRect lMoved = lMask->Rect;
    lMoved.AnchoredPosition = { 300.f, 0.f };
    lMask->SetRect(lMoved);
    lF.Update();

    CHECK(lMask->GetBounds().Center.x == doctest::Approx(300.f));
}

TEST_CASE("UIMask: an EMPTY texture path resolves nothing and is a pure rect clip")
{
    Fixture lF;

    auto lMaskNode = MakeUnique<UIMask>();
    lMaskNode->SetRect(Centred({ 200.f, 200.f }));
    UIMask* const lMask = static_cast<UIMask*>(lF.Canvas.Root().AddChild(Move(lMaskNode)));
    UIImage* const lInside = AddImage(*lMask, "Inside");

    const TDynArray<UIDrawItem> lItems = lF.DrawList();

    CHECK(lF.Assets.Resolves == 0u);                        // never asked — there is no path
    CHECK(lMask->GetResolvedTexture() == nullptr);          // the rect alone clips
    CHECK(ItemFor(lItems, *lInside).Mask == lMask);         // and it still masks
}

TEST_CASE("UIMask: a named texture is resolved through the provider, and re-armed while uploading")
{
    Fixture lF;

    auto lMaskNode = MakeUnique<UIMask>();
    lMaskNode->SetRect(Centred({ 200.f, 200.f }));
    UIMask* const lMask = static_cast<UIMask*>(lF.Canvas.Root().AddChild(Move(lMaskNode)));
    AddImage(*lMask, "Inside");

    lF.Assets.bUploaded = false;
    lMask->SetTexture(OpaaxString("UI/Circle.png"));

    lF.Update();
    CHECK(lF.Assets.LastPath == OpaaxString("UI/Circle.png"));
    CHECK(lMask->GetResolvedTexture() == nullptr);

    const Uint32 lAfterFirst = lF.Assets.Resolves;
    lF.Update();
    CHECK(lF.Assets.Resolves > lAfterFirst);   // asked again — nobody polled

    lF.Assets.bUploaded = true;
    lF.Update();
    CHECK(lMask->GetResolvedTexture() != nullptr);
}

TEST_CASE("UIMask: a hidden mask hides its children, and the mask is not a hit target")
{
    Fixture lF;

    auto lMaskNode = MakeUnique<UIMask>();
    lMaskNode->SetRect(Centred({ 200.f, 200.f }));
    UIMask* const lMask = static_cast<UIMask*>(lF.Canvas.Root().AddChild(Move(lMaskNode)));
    AddImage(*lMask, "Inside");

    CHECK(lF.DrawList().size() == 1u);

    lMask->bVisible = false;
    CHECK(lF.DrawList().empty());   // the walk stops at an invisible node, children included

    // A mask must never swallow a click meant for what it masks.
    lMask->bVisible = true;
    lF.Update();
    CHECK(lF.Canvas.HitTest({ 0.f, 0.f }) != lMask);
}

TEST_CASE("UIImage: an authored texture PATH resolves through the provider; a runtime pointer wins")
{
    Fixture lF;

    UIImage* const lImage = AddImage(lF.Canvas.Root(), "Art");
    lImage->SetTexturePath(OpaaxString("UI/Art.png"));

    lF.Update();
    REQUIRE(lImage->GetQuads().size() == 1u);
    CHECK(lImage->GetQuads()[0].Texture == &lF.Assets.Texture);
    CHECK(lF.Assets.LastPath == OpaaxString("UI/Art.png"));

    // A code-set pointer OVERRULES the path (TX1's precedence), and is not asked for again.
    FakeTexture lRuntime;
    lImage->SetTexture(&lRuntime);
    const Uint32 lBefore = lF.Assets.Resolves;

    lF.Update();
    CHECK(lImage->GetQuads()[0].Texture == &lRuntime);
    CHECK(lF.Assets.Resolves == lBefore);
}

TEST_CASE("UIDrawList: order is tree order, parents before children")
{
    Fixture lF;

    UIWidget* const lPanel = lF.Canvas.Root().AddChild(MakeUnique<UIPanel>());
    UIImage*  const lFirst  = AddImage(*lPanel, "First");
    UIImage*  const lSecond = AddImage(*lPanel, "Second");

    const TDynArray<UIDrawItem> lItems = lF.DrawList();

    REQUIRE(lItems.size() == 2u);
    CHECK(lItems[0].Quad == &lFirst->GetQuads()[0]);
    CHECK(lItems[1].Quad == &lSecond->GetQuads()[0]);
    CHECK(lItems[0].Order < lItems[1].Order);
}
