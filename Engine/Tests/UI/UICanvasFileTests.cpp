// Suite: the `.opaaxui` format (UI/UICanvasFile.h, UI/UIWidgetRegistry.h) — UI12 / UI13.
//
// A widget serializes ITSELF through SaveFields/LoadFields and the registry turns a type tag back
// into one, so what is gated here is the round trip: every field of every type survives, the tree
// shape survives, an unknown tag costs one NODE rather than the file, and a missing key takes the
// default (which is what lets a field be added later without refusing files written before it).
#include <doctest.h>

#include "Engine/Subsystems/Resources/Types/UI/UICanvasResource.h"
#include "UI/UICanvasFile.h"
#include "UI/UIWidgetRegistry.h"
#include "UI/Widgets/UIButton.h"
#include "UI/Widgets/UIImage.h"
#include "UI/Widgets/UIMask.h"
#include "UI/Widgets/UIPanel.h"
#include "UI/Widgets/UIText.h"

using namespace Opaax;

namespace
{
    /** The native types, as Engine::RegisterNativeUIWidgets registers them. */
    UIWidgetRegistry MakeRegistry()
    {
        UIWidgetRegistry lRegistry;
        lRegistry.Register<UIPanel>(OPAAX_ID("UIPanel"));
        lRegistry.Register<UIImage>(OPAAX_ID("UIImage"));
        lRegistry.Register<UIText>(OPAAX_ID("UIText"));
        lRegistry.Register<UIButton>(OPAAX_ID("UIButton"));
        lRegistry.Register<UIMask>(OPAAX_ID("UIMask"));
        return lRegistry;
    }

    UIRect Corner(const Vector2F& InAnchor, const Vector2F& InOffset, const Vector2F& InSize)
    {
        UIRect lRect;
        lRect.AnchorMin = lRect.AnchorMax = lRect.Pivot = InAnchor;
        lRect.AnchoredPosition = InOffset;
        lRect.SizeDelta        = InSize;
        return lRect;
    }

    void CheckVec(const Vector2F& InA, const Vector2F& InB)
    {
        CHECK(InA.x == doctest::Approx(InB.x));
        CHECK(InA.y == doctest::Approx(InB.y));
    }
}

TEST_CASE("UICanvasFile: every widget type round-trips its own fields and the tree shape")
{
    const UIWidgetRegistry lRegistry = MakeRegistry();

    UICanvasFile::UICanvasDoc lDoc;
    lDoc.ReferenceHeight = 720.f;
    lDoc.Root            = MakeUnique<UIPanel>();
    lDoc.Root->Name      = "Root";

    UIWidget* const lPanel = lDoc.Root->AddChild(MakeUnique<UIPanel>());
    lPanel->Name         = "Hud";
    lPanel->bVisible     = false;
    lPanel->bHitTestable = true;
    lPanel->SetRect(Corner({ 0.f, 1.f }, { 24.f, -24.f }, { 600.f, 60.f }));

    auto lText = MakeUnique<UIText>();
    lText->Name            = "Jumps";
    lText->Text            = "Jumps: 0";
    lText->Font.Path       = "/Engine/Fonts/Roboto/roboto-latin-700-normal.ttf";
    lText->Size            = 40.f;
    lText->Color           = { 0.1f, 0.2f, 0.3f, 0.9f };
    lText->HAlign          = ETextAlign::Right;
    lText->VAlign          = EUIVAlign::Middle;
    lText->bWrap           = false;
    lText->LineHeightScale = 1.25f;
    lText->bKerning        = false;
    lPanel->AddChild(Move(lText));

    auto lImage = MakeUnique<UIImage>();
    lImage->Name       = "SpeedFill";
    lImage->Color      = { 0.2f, 0.85f, 0.35f, 1.f };
    lImage->Fill       = EUIFill::Horizontal;
    lImage->FillAmount = 0.5f;
    lPanel->AddChild(Move(lImage));

    auto lButton = MakeUnique<UIButton>();
    lButton->Name     = "Menu";
    lButton->Normal   = { 0.11f, 0.f, 0.f, 1.f };
    lButton->Hovered  = { 0.22f, 0.f, 0.f, 1.f };
    lButton->Pressed  = { 0.33f, 0.f, 0.f, 1.f };
    lButton->Disabled = { 0.44f, 0.f, 0.f, 0.5f };
    lButton->bEnabled = false;
    lDoc.Root->AddChild(Move(lButton));

    const OpaaxString lText1 = UICanvasFile::Serialize(lDoc);

    UICanvasFile::UICanvasDoc lBack;
    REQUIRE(UICanvasFile::Deserialize(lText1, lRegistry, lBack));
    REQUIRE(lBack.Root != nullptr);

    CHECK(lBack.ReferenceHeight == doctest::Approx(720.f));
    CHECK(UICanvasFile::CountWidgets(*lBack.Root) == 5u);   // root + panel + text + image + button

    // The SHAPE: the panel kept its two children, the button stayed a sibling of the panel.
    REQUIRE(lBack.Root->GetChildren().size() == 2u);
    const UIWidget& lBackPanel = *lBack.Root->GetChildren()[0];
    REQUIRE(lBackPanel.GetChildren().size() == 2u);

    CHECK(lBackPanel.Name == OpaaxString("Hud"));
    CHECK_FALSE(lBackPanel.bVisible);
    CHECK(lBackPanel.bHitTestable);
    CheckVec(lBackPanel.Rect.SizeDelta, { 600.f, 60.f });
    CheckVec(lBackPanel.Rect.AnchoredPosition, { 24.f, -24.f });

    const auto* lBackText = dynamic_cast<const UIText*>(lBackPanel.GetChildren()[0].get());
    REQUIRE(lBackText != nullptr);
    CHECK(lBackText->Text == OpaaxString("Jumps: 0"));
    CHECK(lBackText->Font.Path == OpaaxString("/Engine/Fonts/Roboto/roboto-latin-700-normal.ttf"));
    CHECK(lBackText->Size == doctest::Approx(40.f));
    CHECK(lBackText->Color.b == doctest::Approx(0.3f));
    CHECK(lBackText->HAlign == ETextAlign::Right);
    CHECK(lBackText->VAlign == EUIVAlign::Middle);
    CHECK_FALSE(lBackText->bWrap);
    CHECK(lBackText->LineHeightScale == doctest::Approx(1.25f));
    CHECK_FALSE(lBackText->bKerning);

    const auto* lBackImage = dynamic_cast<const UIImage*>(lBackPanel.GetChildren()[1].get());
    REQUIRE(lBackImage != nullptr);
    CHECK(lBackImage->Fill == EUIFill::Horizontal);
    CHECK(lBackImage->FillAmount == doctest::Approx(0.5f));
    CHECK(lBackImage->Color.g == doctest::Approx(0.85f));

    const auto* lBackButton = dynamic_cast<const UIButton*>(lBack.Root->GetChildren()[1].get());
    REQUIRE(lBackButton != nullptr);
    CHECK_FALSE(lBackButton->bEnabled);
    CHECK(lBackButton->Pressed.r == doctest::Approx(0.33f));

    // And the text is STABLE: writing what was read gives the same bytes, which is what makes a
    // dirty check a string compare (**UI15**) instead of a tree diff.
    CHECK(UICanvasFile::Serialize(lBack) == lText1);
}

TEST_CASE("UICanvasFile: an unknown type costs that NODE, not the file")
{
    const UIWidgetRegistry lRegistry = MakeRegistry();

    // "UIHologram" is a type this build does not know; its SIBLING must still arrive.
    const OpaaxString lText = OpaaxString(R"({
    "ReferenceHeight": 1080.0,
    "Root": {
        "Type": "UIPanel",
        "Children": [
            { "Type": "UIHologram", "Name": "FromTheFuture" },
            { "Type": "UIText", "Name": "Kept" }
        ]
    },
    "Version": 1
})");

    UICanvasFile::UICanvasDoc lDoc;
    REQUIRE(UICanvasFile::Deserialize(lText, lRegistry, lDoc));
    REQUIRE(lDoc.Root != nullptr);

    REQUIRE(lDoc.Root->GetChildren().size() == 1u);
    CHECK(lDoc.Root->GetChildren()[0]->Name == OpaaxString("Kept"));
}

TEST_CASE("UICanvasFile: a missing key takes the constructed default — a field may be added later")
{
    const UIWidgetRegistry lRegistry = MakeRegistry();

    // A text node with NOTHING but its type: every field must land on its default rather than throw.
    const OpaaxString lText = OpaaxString(R"({
    "Root": { "Type": "UIPanel", "Children": [ { "Type": "UIText" } ] },
    "Version": 1
})");

    UICanvasFile::UICanvasDoc lDoc;
    REQUIRE(UICanvasFile::Deserialize(lText, lRegistry, lDoc));

    const UIText  lFresh;
    const auto*   lRead = dynamic_cast<const UIText*>(lDoc.Root->GetChildren()[0].get());
    REQUIRE(lRead != nullptr);

    CHECK(lRead->Size == doctest::Approx(lFresh.Size));
    CHECK(lRead->bWrap == lFresh.bWrap);
    CHECK(lRead->HAlign == lFresh.HAlign);
    CHECK(lRead->bVisible == lFresh.bVisible);
    CheckVec(lRead->Rect.SizeDelta, lFresh.Rect.SizeDelta);

    // The whole file missing ReferenceHeight takes the default too.
    CHECK(lDoc.ReferenceHeight == doctest::Approx(1080.f));
}

TEST_CASE("UICanvasFile: malformed text and a future version are REFUSED, leaving the doc untouched")
{
    const UIWidgetRegistry lRegistry = MakeRegistry();

    UICanvasFile::UICanvasDoc lDoc;
    lDoc.ReferenceHeight = 999.f;

    CHECK_FALSE(UICanvasFile::Deserialize(OpaaxString("{ not json"), lRegistry, lDoc));
    CHECK_FALSE(UICanvasFile::Deserialize(OpaaxString(R"({"Version": 99, "Root": {"Type": "UIPanel"}})"),
                                          lRegistry, lDoc));

    // A root whose TYPE is unknown leaves nothing to build a tree from.
    CHECK_FALSE(UICanvasFile::Deserialize(OpaaxString(R"({"Version": 1, "Root": {"Type": "Nope"}})"),
                                          lRegistry, lDoc));

    CHECK(lDoc.ReferenceHeight == doctest::Approx(999.f));   // untouched on every failure path
}

TEST_CASE("UIWidgetRegistry: an unknown name builds nothing, a duplicate is refused, a seal closes it")
{
    UIWidgetRegistry lRegistry = MakeRegistry();

    CHECK(lRegistry.Count() == 5u);
    CHECK(lRegistry.IsRegistered(OPAAX_ID("UIText")));
    CHECK_FALSE(lRegistry.IsRegistered(OPAAX_ID("UIHologram")));
    CHECK(lRegistry.Create(OPAAX_ID("UIHologram")) == nullptr);
    CHECK(lRegistry.Create(OPAAX_ID("UIText")) != nullptr);

    CHECK_FALSE(lRegistry.Register<UIText>(OPAAX_ID("UIText")));   // the name is taken
    CHECK(lRegistry.Count() == 5u);

    lRegistry.Seal();
    CHECK_FALSE(lRegistry.Register<UIPanel>(OPAAX_ID("UILater")));
    CHECK(lRegistry.Count() == 5u);
}

TEST_CASE("UIWidget: FindByName takes the FIRST match in tree order, and misses answer null")
{
    UIPanel lRoot;
    lRoot.Name = "Root";

    UIWidget* lFirst = lRoot.AddChild(MakeUnique<UIPanel>());
    lFirst->Name = "Twin";
    UIWidget* lDeep = lFirst->AddChild(MakeUnique<UIText>());
    lDeep->Name = "Deep";

    UIWidget* lSecond = lRoot.AddChild(MakeUnique<UIPanel>());
    lSecond->Name = "Twin";

    CHECK(lRoot.FindByName(OpaaxString("Root")) == &lRoot);
    CHECK(lRoot.FindByName(OpaaxString("Twin")) == lFirst);   // depth-first, first wins
    CHECK(lRoot.FindByName(OpaaxString("Deep")) == lDeep);
    CHECK(lRoot.FindByName(OpaaxString("Nobody")) == nullptr);
}

TEST_CASE("UICanvasResource: BuildTree yields an INDEPENDENT tree every time (UI13)")
{
    const UIWidgetRegistry lRegistry = MakeRegistry();

    UICanvasResource lResource;
    lResource.Text = OpaaxString(R"({
    "ReferenceHeight": 720.0,
    "Root": { "Type": "UIPanel", "Children": [ { "Type": "UIText", "Name": "Label" } ] },
    "Version": 1
})");

    float lHeightA = 0.f;
    float lHeightB = 0.f;
    TUniquePtr<UIWidget> lA = lResource.BuildTree(lRegistry, lHeightA);
    TUniquePtr<UIWidget> lB = lResource.BuildTree(lRegistry, lHeightB);

    REQUIRE(lA != nullptr);
    REQUIRE(lB != nullptr);
    CHECK(lA.get() != lB.get());
    CHECK(lHeightA == doctest::Approx(720.f));
    CHECK(lHeightB == doctest::Approx(720.f));

    // Editing one must not reach the other — the property the resource holds TEXT for.
    lA->FindByName(OpaaxString("Label"))->Name = "Renamed";
    CHECK(lB->FindByName(OpaaxString("Label")) != nullptr);
    CHECK(lB->FindByName(OpaaxString("Renamed")) == nullptr);

    // A placeholder builds a real (empty) tree rather than a null — a failed load still draws.
    float lPlaceholderHeight = 0.f;
    CHECK(UICanvasResource::Placeholder().BuildTree(lRegistry, lPlaceholderHeight) != nullptr);
}

TEST_CASE("UICanvasFile: a file written when the asset fields were STRINGS still reads (UI19)")
{
    // The zero-format-change claim, asserted rather than trusted. TResourcePath serializes as a
    // BARE STRING (ResourcePathJson.h), so a `.opaaxui` authored before the fields became typed
    // must load with its paths intact — no migration, no version bump.
    const UIWidgetRegistry lRegistry = MakeRegistry();

    const OpaaxString lLegacy = OpaaxString(R"({
    "ReferenceHeight": 1080.0,
    "Root": {
        "Type": "UIPanel",
        "Children": [
            { "Type": "UIText",  "Name": "Label", "Font": "/Engine/Fonts/Roboto/roboto-latin-700-normal.ttf" },
            { "Type": "UIImage", "Name": "Art",   "Texture": "UI/Art.png" },
            { "Type": "UIMask",  "Name": "Cut",   "Texture": "UI/MaskDisc.png" }
        ]
    },
    "Version": 1
})");

    UICanvasFile::UICanvasDoc lDoc;
    REQUIRE(UICanvasFile::Deserialize(lLegacy, lRegistry, lDoc));
    REQUIRE(lDoc.Root != nullptr);
    REQUIRE(lDoc.Root->GetChildren().size() == 3u);

    const auto* lText  = dynamic_cast<const UIText*>(lDoc.Root->GetChildren()[0].get());
    const auto* lImage = dynamic_cast<const UIImage*>(lDoc.Root->GetChildren()[1].get());
    const auto* lMask  = dynamic_cast<const UIMask*>(lDoc.Root->GetChildren()[2].get());
    REQUIRE(lText  != nullptr);
    REQUIRE(lImage != nullptr);
    REQUIRE(lMask  != nullptr);

    CHECK(lText->Font.Path == OpaaxString("/Engine/Fonts/Roboto/roboto-latin-700-normal.ttf"));
    CHECK(lImage->Texture.Path == OpaaxString("UI/Art.png"));
    CHECK(lMask->Texture.Path == OpaaxString("UI/MaskDisc.png"));

    // And it writes back as bare strings, so the file does not churn on the next save.
    const OpaaxString lText2 = UICanvasFile::Serialize(lDoc);
    CHECK(lText2.Find("\"Font\": \"/Engine/Fonts/Roboto/roboto-latin-700-normal.ttf\"") >= 0);
}
