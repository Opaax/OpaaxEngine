// Suite: the .opaaxui format (UI/UICanvasFile.h). Every field of every widget class and the tree
// shape survive a round trip; an unknown type costs one node; a missing key keeps its default. And
// UICanvasAsset, the file loaded as an asset.
#include <doctest.h>

#include "../TestTempDir.h"
#include "Core/IO/FileIO.h"
#include "Resources/Asset/AssetResource.h"
#include "Resources/ResourceManager.h"
#include "UI/UICanvasAsset.h"
#include "UI/UICanvasFile.h"
#include "Object/ObjectClassRegistry.h"
#include "UI/Widgets/UIButton.h"
#include "UI/Widgets/UIImage.h"
#include "UI/Widgets/UIMask.h"
#include "UI/Widgets/UIPanel.h"
#include "UI/Widgets/UISafeArea.h"
#include "UI/Widgets/UIStack.h"
#include "UI/Widgets/UIText.h"

using namespace Opaax;

namespace
{
    /** The engine's widget classes, as UIRegistration.cpp registers them. */
    struct WidgetClasses final : ObjectClassRegistry
    {
        WidgetClasses()
        {
            Register<UIWidget>();
            Register<UIPanel>();
            Register<UIImage>();
            Register<UIText>();
            Register<UIButton>();
            Register<UIMask>();
            Register<UISafeArea>();
            Register<UIStack>();
        }
    };

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
    const WidgetClasses lRegistry;

    UICanvasFile::UICanvasDoc lDoc;
    lDoc.ReferenceHeight = 720.f;
    lDoc.Root            = NewObject<UIPanel>();
    lDoc.Root->Name      = "Root";

    UIWidget* const lPanel = lDoc.Root->AddChild(NewObject<UIPanel>());
    lPanel->Name         = "Hud";
    lPanel->bVisible     = false;
    lPanel->bHitTestable = true;
    lPanel->SetRect(Corner({ 0.f, 1.f }, { 24.f, -24.f }, { 600.f, 60.f }));

    auto lText = NewObject<UIText>();
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

    auto lImage = NewObject<UIImage>();
    lImage->Name       = "SpeedFill";
    lImage->Brush.Tint = { 0.2f, 0.85f, 0.35f, 1.f };
    lImage->Fill       = EUIFill::Horizontal;
    lImage->FillAmount = 0.5f;
    lPanel->AddChild(Move(lImage));

    auto lButton = NewObject<UIButton>();
    lButton->Name     = "Menu";
    lButton->Normal   = { 0.11f, 0.f, 0.f, 1.f };
    lButton->Hovered  = { 0.22f, 0.f, 0.f, 1.f };
    lButton->Pressed  = { 0.33f, 0.f, 0.f, 1.f };
    lButton->Disabled = { 0.44f, 0.f, 0.f, 0.5f };
    lButton->SetEnabled(false);
    lDoc.Root->AddChild(Move(lButton));

    const OpaaxString lText1 = UICanvasFile::Serialize(lDoc);
    CHECK(lText1.Contains("0.85,"));                  // a colour as typed, not 0.8500000238418579
    CHECK_FALSE(lText1.Contains("0.8500000"));

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

    const auto* lBackText = dynamic_cast<const UIText*>(lBackPanel.GetChildren()[0].Get());
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

    const auto* lBackImage = dynamic_cast<const UIImage*>(lBackPanel.GetChildren()[1].Get());
    REQUIRE(lBackImage != nullptr);
    CHECK(lBackImage->Fill == EUIFill::Horizontal);
    CHECK(lBackImage->FillAmount == doctest::Approx(0.5f));
    CHECK(lBackImage->Brush.Tint.g == doctest::Approx(0.85f));

    const auto* lBackButton = dynamic_cast<const UIButton*>(lBack.Root->GetChildren()[1].Get());
    REQUIRE(lBackButton != nullptr);
    CHECK_FALSE(lBackButton->IsEnabled());
    CHECK(lBackButton->Pressed.r == doctest::Approx(0.33f));

    // And the text is STABLE: writing what was read gives the same bytes, which is what makes a
    // dirty check a string compare instead of a tree diff.
    CHECK(UICanvasFile::Serialize(lBack) == lText1);
}

TEST_CASE("UICanvasFile: a new canvas's root fills the screen it is mounted on")
{
    // A HUD anchors its widgets to the screen's corners: they are the root's corners.
    const UICanvasFile::UICanvasDoc lDoc = UICanvasFile::MakeEmpty(720.f);
    REQUIRE(lDoc.Root != nullptr);
    CHECK(lDoc.ReferenceHeight == 720.f);
    CHECK(lDoc.Root->GetChildren().empty());

    const Bounds2D lScreen = Bounds2D::FromMinMax({ 0.f, 0.f }, { 1280.f, 720.f });
    const Bounds2D lRoot   = ResolveRect(lDoc.Root->Rect, lScreen);
    CHECK(lRoot.Min().x == doctest::Approx(0.f));
    CHECK(lRoot.Min().y == doctest::Approx(0.f));
    CHECK(lRoot.Max().x == doctest::Approx(1280.f));
    CHECK(lRoot.Max().y == doctest::Approx(720.f));

    // The empty screen around the widgets lets clicks through to the game.
    CHECK_FALSE(lDoc.Root->bHitTestable);

    // What the file keeps is what was made.
    UICanvasFile::UICanvasDoc lRead;
    REQUIRE(UICanvasFile::Deserialize(UICanvasFile::Serialize(lDoc), WidgetClasses(), lRead));
    CHECK(lRead.Root->Rect.AnchorMax.x == doctest::Approx(1.f));
    CHECK(lRead.Root->Rect.SizeDelta.x == doctest::Approx(0.f));
}

TEST_CASE("UICanvasFile: an unknown type costs that NODE, not the file")
{
    const WidgetClasses lRegistry;

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
    const WidgetClasses lRegistry;

    // A text node with NOTHING but its type: every field must land on its default rather than throw.
    const OpaaxString lText = OpaaxString(R"({
    "Root": { "Type": "UIPanel", "Children": [ { "Type": "UIText" } ] },
    "Version": 1
})");

    UICanvasFile::UICanvasDoc lDoc;
    REQUIRE(UICanvasFile::Deserialize(lText, lRegistry, lDoc));

    const UIText  lFresh{};
    const auto*   lRead = dynamic_cast<const UIText*>(lDoc.Root->GetChildren()[0].Get());
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
    const WidgetClasses lRegistry;

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

TEST_CASE("UIWidget: FindByName takes the FIRST match in tree order, and misses answer null")
{
    UIPanel lRoot;
    lRoot.Name = "Root";

    UIWidget* lFirst = lRoot.AddChild(NewObject<UIPanel>());
    lFirst->Name = "Twin";
    UIWidget* lDeep = lFirst->AddChild(NewObject<UIText>());
    lDeep->Name = "Deep";

    UIWidget* lSecond = lRoot.AddChild(NewObject<UIPanel>());
    lSecond->Name = "Twin";

    CHECK(lRoot.FindByName(OpaaxString("Root")) == &lRoot);
    CHECK(lRoot.FindByName(OpaaxString("Twin")) == lFirst);   // depth-first, first wins
    CHECK(lRoot.FindByName(OpaaxString("Deep")) == lDeep);
    CHECK(lRoot.FindByName(OpaaxString("Nobody")) == nullptr);
}

TEST_CASE("UICanvasAsset: BuildTree yields an INDEPENDENT tree every time")
{
    const TestTempDir   lTemp("canvas_asset");
    const WidgetClasses lRegistry;

    const OpaaxString lPath = lTemp.Sub("Hud.opaaxui");
    REQUIRE(FileIO::WriteAllText(lPath, OpaaxString(R"({
    "ReferenceHeight": 720.0,
    "Root": { "Type": "UIPanel", "Children": [ { "Type": "UIText", "Name": "Label" } ] },
    "Version": 1
})")));

    UICanvasAsset lCanvas;
    REQUIRE(lCanvas.LoadSource(lPath.CStr()));
    CHECK(lCanvas.GetResourceSize() > sizeof(UICanvasAsset));

    float lHeightA = 0.f;
    float lHeightB = 0.f;
    TStrongObjectPtr<UIWidget> lA = lCanvas.BuildTree(lRegistry, lHeightA);
    TStrongObjectPtr<UIWidget> lB = lCanvas.BuildTree(lRegistry, lHeightB);

    REQUIRE(lA != nullptr);
    REQUIRE(lB != nullptr);
    CHECK(lA.Get() != lB.Get());
    CHECK(lHeightA == doctest::Approx(720.f));
    CHECK(lHeightB == doctest::Approx(720.f));

    // Editing one must not reach the other — the property the asset holds TEXT for.
    lA->FindByName(OpaaxString("Label"))->Name = "Renamed";
    CHECK(lB->FindByName(OpaaxString("Label")) != nullptr);
    CHECK(lB->FindByName(OpaaxString("Renamed")) == nullptr);
}

TEST_CASE("UICanvasAsset: a canvas file loads as a UICanvasAsset; a missing or empty one gives none")
{
    const TestTempDir lTemp("canvas_resource");

    const OpaaxString lPath  = lTemp.Sub("Menu.opaaxui");
    const OpaaxString lEmpty = lTemp.Sub("Empty.opaaxui");
    REQUIRE(FileIO::WriteAllText(lPath,
        OpaaxString(R"({ "ReferenceHeight": 1080.0, "Root": { "Type": "UIPanel" }, "Version": 1 })")));
    REQUIRE(FileIO::WriteAllText(lEmpty, OpaaxString()));

    ObjectClassRegistry lClasses;
    REQUIRE(lClasses.Register<UICanvasAsset>());

    ResourceManager lResources;
    lResources.SetObjectClasses(&lClasses);

    const ResourceRef<AssetResource> lRef    = lResources.Load<AssetResource>(lPath.CStr());
    const UICanvasAsset* const       lCanvas = lRef.Get() != nullptr ? lRef.Get()->Get<UICanvasAsset>() : nullptr;
    REQUIRE(lCanvas != nullptr);
    CHECK(lCanvas->GetName() == OPAAX_ID("Menu"));

    const WidgetClasses lWidgets;
    float               lHeight = 0.f;
    CHECK(lCanvas->BuildTree(lWidgets, lHeight) != nullptr);
    CHECK(lHeight == doctest::Approx(1080.f));

    CHECK_FALSE(lResources.Load<AssetResource>(lEmpty.CStr()).IsValid());
    CHECK_FALSE(lResources.Load<AssetResource>(lTemp.Sub("Missing.opaaxui").CStr()).IsValid());

    lResources.FlushAll();
}

TEST_CASE("UICanvasFile: U5b's two new fields round-trip, and a file without them keeps the defaults")
{
    const WidgetClasses lRegistry;

    UICanvasFile::UICanvasDoc lDoc;
    lDoc.Root = NewObject<UIPanel>();

    auto lImage = NewObject<UIImage>();
    lImage->Name   = "Frame";
    lImage->Brush.Border = UIMargin{ 12.f, 13.f, 14.f, 15.f };
    lDoc.Root->AddChild(Move(lImage));

    auto lSafe = NewObject<UISafeArea>();
    lSafe->Name   = "Safe";
    lSafe->Insets = UIMargin{ 0.1f, 0.2f, 0.3f, 0.4f };
    lDoc.Root->AddChild(Move(lSafe));

    UICanvasFile::UICanvasDoc lBack;
    REQUIRE(UICanvasFile::Deserialize(UICanvasFile::Serialize(lDoc), lRegistry, lBack));

    const auto* lBackImage = dynamic_cast<const UIImage*>(lBack.Root->GetChildren()[0].Get());
    const auto* lBackSafe  = dynamic_cast<const UISafeArea*>(lBack.Root->GetChildren()[1].Get());
    REQUIRE(lBackImage != nullptr);
    REQUIRE(lBackSafe  != nullptr);

    CHECK(lBackImage->Brush.Border.Left  == doctest::Approx(12.f));
    CHECK(lBackImage->Brush.Border.Right == doctest::Approx(13.f));
    CHECK(lBackImage->Brush.Border.Bottom == doctest::Approx(14.f));
    CHECK(lBackImage->Brush.Border.Top   == doctest::Approx(15.f));
    CHECK(lBackSafe->Insets.Left == doctest::Approx(0.1f));
    CHECK(lBackSafe->Insets.Top  == doctest::Approx(0.4f));

    // AND THE FORMAT DID NOT CHANGE: an image node written before U5b names no Border, which must
    // take the default rather than throw — the reason every field reads through _WITH_DEFAULT.
    const OpaaxString lBefore = OpaaxString(R"({
    "Root": { "Type": "UIPanel", "Children": [ { "Type": "UIImage", "Name": "Old" } ] },
    "Version": 1
})");

    UICanvasFile::UICanvasDoc lOld;
    REQUIRE(UICanvasFile::Deserialize(lBefore, lRegistry, lOld));

    const auto* lOldImage = dynamic_cast<const UIImage*>(lOld.Root->GetChildren()[0].Get());
    REQUIRE(lOldImage != nullptr);
    CHECK(lOldImage->Brush.Border.IsZero());
}

TEST_CASE("UICanvasFile: a file written when the asset fields were STRINGS still reads")
{
    // The zero-format-change claim, asserted rather than trusted. TAssetRef serializes as a
    // BARE STRING (AssetRef.h), so a `.opaaxui` authored before the fields became typed
    // must load with its paths intact — no migration, no version bump.
    const WidgetClasses lRegistry;

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

    const auto* lText  = dynamic_cast<const UIText*>(lDoc.Root->GetChildren()[0].Get());
    const auto* lImage = dynamic_cast<const UIImage*>(lDoc.Root->GetChildren()[1].Get());
    const auto* lMask  = dynamic_cast<const UIMask*>(lDoc.Root->GetChildren()[2].Get());
    REQUIRE(lText  != nullptr);
    REQUIRE(lImage != nullptr);
    REQUIRE(lMask  != nullptr);

    CHECK(lText->Font.Path == OpaaxString("/Engine/Fonts/Roboto/roboto-latin-700-normal.ttf"));
    CHECK(lImage->Brush.Texture.Path == OpaaxString("UI/Art.png"));
    CHECK(lMask->Texture.Path == OpaaxString("UI/MaskDisc.png"));

    // And it writes back as bare strings, so the file does not churn on the next save.
    const OpaaxString lText2 = UICanvasFile::Serialize(lDoc);
    CHECK(lText2.Find("\"Font\": \"/Engine/Fonts/Roboto/roboto-latin-700-normal.ttf\"") >= 0);
}

TEST_CASE("UICanvasFile: CloneWidget is a deep copy through the format, and a node's text pastes back")
{
    const WidgetClasses lRegistry;

    // A button with a label under it: two types, two levels, one field of each kind.
    auto lButton      = NewObject<UIButton>();
    lButton->Name     = "Resume";
    lButton->SetEnabled(false);
    lButton->SetRect(Corner({ 0.5f, 0.f }, { 0.f, 30.f }, { 240.f, 64.f }));

    auto lLabel  = NewObject<UIText>();
    lLabel->Name = "ResumeLabel";
    lLabel->Text = "Resume";
    lButton->AddChild(Move(lLabel));

    TStrongObjectPtr<UIWidget> lClone = UICanvasFile::CloneWidget(*lButton, lRegistry);
    REQUIRE(lClone != nullptr);
    CHECK(lClone.Get() != lButton.Get());
    CHECK(lClone->GetParent() == nullptr);   // detached, whoever asked will place it

    const auto* lCloned = dynamic_cast<const UIButton*>(lClone.Get());
    REQUIRE(lCloned != nullptr);
    CHECK(lCloned->Name == OpaaxString("Resume"));
    CHECK_FALSE(lCloned->IsEnabled());
    CheckVec(lCloned->Rect.SizeDelta, { 240.f, 64.f });
    REQUIRE(lCloned->GetChildren().size() == 1u);
    CHECK(lCloned->GetChildren()[0]->Name == OpaaxString("ResumeLabel"));
    CHECK(lCloned->GetChildren()[0].Get() != lButton->GetChildren()[0].Get());

    // The clipboard's unit: the same text the file would hold for that node, read back alone.
    const OpaaxString lText = UICanvasFile::SerializeNode(*lButton);
    CHECK(lText.Find("\"Type\": \"UIButton\"") >= 0);

    TStrongObjectPtr<UIWidget> lPasted = UICanvasFile::DeserializeNode(lText, lRegistry);
    REQUIRE(lPasted != nullptr);
    CHECK(lPasted->Name == OpaaxString("Resume"));
    CHECK(lPasted->GetChildren().size() == 1u);

    // Not a node: a paste of unrelated clipboard text is a no-op, not a crash.
    CHECK(UICanvasFile::DeserializeNode(OpaaxString("hello"), lRegistry) == nullptr);
    CHECK(UICanvasFile::DeserializeNode(OpaaxString("[1, 2]"), lRegistry) == nullptr);
}

TEST_CASE("UIWidget: AddChild at an index inserts there, and past the end appends")
{
    UIPanel lParent;

    UIWidget* const lA = lParent.AddChild(NewObject<UIPanel>());
    UIWidget* const lC = lParent.AddChild(NewObject<UIPanel>());
    UIWidget* const lB = lParent.AddChild(NewObject<UIPanel>(), 1);   // between A and C
    UIWidget* const lD = lParent.AddChild(NewObject<UIPanel>(), 99);  // clamped to the end

    REQUIRE(lParent.GetChildren().size() == 4u);
    CHECK(lParent.GetChildren()[0].Get() == lA);
    CHECK(lParent.GetChildren()[1].Get() == lB);
    CHECK(lParent.GetChildren()[2].Get() == lC);
    CHECK(lParent.GetChildren()[3].Get() == lD);
    CHECK(lB->GetParent() == &lParent);
}

// =============================================================================
// Modifiers in a node
// =============================================================================

namespace
{
    /** A modifier with a field of its own, for the file's round trips. */
    class UIFileTestModifier final : public UIModifier
    {
        OPAAX_OBJECT(UIFileTestModifier, UIModifier)

    public:
        float    Strength = 1.f;
        Vector2F Offset   = { 0.f, 0.f };

        OPAAX_PROPERTIES(UIFileTestModifier, OPAAX_PROP(Strength), OPAAX_PROP(Offset))
    };

    /** The widget classes, and the modifier classes a node may name. */
    struct ModifierClasses final : ObjectClassRegistry
    {
        ModifierClasses()
        {
            Register<UIWidget>();
            Register<UIPanel>();
            Register<UIImage>();
            Register<UIModifier>();
            Register<UIFileTestModifier>();
        }
    };

    const UIFileTestModifier* ModifierAt(const UIWidget& InWidget, const Uint64 InIndex)
    {
        return InIndex < InWidget.GetModifiers().size()
                   ? Cast<UIFileTestModifier>(InWidget.GetModifiers()[InIndex].Get())
                   : nullptr;
    }
}

TEST_CASE("UICanvasFile: a node's modifiers round-trip in order, with their fields")
{
    const ModifierClasses lRegistry;

    UICanvasFile::UICanvasDoc lDoc = UICanvasFile::MakeEmpty(1080.f);
    UIWidget* const lImage = lDoc.Root->AddChild(NewObject<UIImage>());
    lImage->Name = "Badge";

    UIFileTestModifier& lFirst = lImage->AddModifier<UIFileTestModifier>();
    lFirst.Name     = OPAAX_ID("Glow");
    lFirst.Strength = 3.5f;
    lFirst.Offset   = { 4.f, -4.f };

    UIFileTestModifier& lSecond = lImage->AddModifier<UIFileTestModifier>();
    lSecond.bEnabled = false;

    const OpaaxString lText = UICanvasFile::Serialize(lDoc);
    CHECK(lText.Find("\"Modifiers\"") >= 0);

    UICanvasFile::UICanvasDoc lBack;
    REQUIRE(UICanvasFile::Deserialize(lText, lRegistry, lBack));

    const UIWidget& lRead = *lBack.Root->GetChildren()[0];
    REQUIRE(lRead.GetModifiers().size() == 2u);

    const UIFileTestModifier* lReadFirst  = ModifierAt(lRead, 0);
    const UIFileTestModifier* lReadSecond = ModifierAt(lRead, 1);
    REQUIRE(lReadFirst != nullptr);
    REQUIRE(lReadSecond != nullptr);

    CHECK(lReadFirst->Name == OPAAX_ID("Glow"));
    CHECK(lReadFirst->Strength == doctest::Approx(3.5f));
    CheckVec(lReadFirst->Offset, { 4.f, -4.f });
    CHECK(lReadFirst->bEnabled);
    CHECK(lReadFirst->GetOwner() == &lRead);

    CHECK_FALSE(lReadSecond->Name.IsValid());
    CHECK_FALSE(lReadSecond->bEnabled);

    // Written again, the same text: nothing is lost or reordered.
    CHECK(UICanvasFile::Serialize(lBack) == lText);
}

TEST_CASE("UICanvasFile: a node without modifiers writes no key, and a file without the key has none")
{
    const ModifierClasses lRegistry;

    UICanvasFile::UICanvasDoc lDoc = UICanvasFile::MakeEmpty(1080.f);
    lDoc.Root->AddChild(NewObject<UIImage>());
    CHECK(UICanvasFile::Serialize(lDoc).Find("Modifiers") < 0);

    const OpaaxString lOld = OpaaxString(R"({
    "Root": { "Type": "UIPanel", "Children": [ { "Type": "UIImage", "Name": "Old" } ] },
    "Version": 1
})");

    UICanvasFile::UICanvasDoc lBack;
    REQUIRE(UICanvasFile::Deserialize(lOld, lRegistry, lBack));
    REQUIRE(lBack.Root->GetChildren().size() == 1u);
    CHECK(lBack.Root->GetChildren()[0]->GetModifiers().empty());
}

TEST_CASE("UICanvasFile: an unknown modifier costs that modifier, not the widget")
{
    const ModifierClasses lRegistry;

    const OpaaxString lText = OpaaxString(R"({
    "Root": { "Type": "UIPanel", "Children": [ { "Type": "UIImage", "Name": "Kept", "Modifiers": [
        { "Type": "UIFileTestModifier", "Strength": 2.0 },
        { "Type": "UIFromTheFuture", "Wobble": 9 },
        "not an object",
        { "Type": "UIFileTestModifier", "Strength": 7.0 }
    ] } ] },
    "Version": 1
})");

    UICanvasFile::UICanvasDoc lDoc;
    REQUIRE(UICanvasFile::Deserialize(lText, lRegistry, lDoc));

    const UIWidget& lKept = *lDoc.Root->GetChildren()[0];
    CHECK(lKept.Name == OpaaxString("Kept"));
    REQUIRE(lKept.GetModifiers().size() == 2u);
    CHECK(ModifierAt(lKept, 0)->Strength == doctest::Approx(2.f));
    CHECK(ModifierAt(lKept, 1)->Strength == doctest::Approx(7.f));
}

TEST_CASE("UICanvasFile: a modifier's value of the wrong type refuses the file")
{
    const ModifierClasses lRegistry;

    UICanvasFile::UICanvasDoc lDoc;
    lDoc.ReferenceHeight = 999.f;

    CHECK_FALSE(UICanvasFile::Deserialize(OpaaxString(R"({
    "Root": { "Type": "UIPanel", "Modifiers": [ { "Type": "UIFileTestModifier", "Strength": "very" } ] },
    "Version": 1
})"), lRegistry, lDoc));

    CHECK(lDoc.ReferenceHeight == doctest::Approx(999.f));
}

TEST_CASE("UICanvasFile: CloneWidget and a node's text carry the modifiers")
{
    const ModifierClasses lRegistry;

    TStrongObjectPtr<UIPanel> lPanel = NewObject<UIPanel>();
    lPanel->AddModifier<UIFileTestModifier>().Strength = 5.f;
    UIWidget* const lChild = lPanel->AddChild(NewObject<UIImage>());
    lChild->AddModifier<UIFileTestModifier>().Strength = 6.f;

    TStrongObjectPtr<UIWidget> lClone = UICanvasFile::CloneWidget(*lPanel, lRegistry);
    REQUIRE(lClone != nullptr);
    REQUIRE(ModifierAt(*lClone, 0) != nullptr);
    CHECK(ModifierAt(*lClone, 0)->Strength == doctest::Approx(5.f));
    CHECK(ModifierAt(*lClone->GetChildren()[0], 0)->Strength == doctest::Approx(6.f));

    // A copy, not the same objects.
    CHECK(lClone->GetModifiers()[0].Get() != lPanel->GetModifiers()[0].Get());
    CHECK(lClone->GetModifiers()[0]->GetOwner() == lClone.Get());

    TStrongObjectPtr<UIWidget> lPasted = UICanvasFile::DeserializeNode(UICanvasFile::SerializeNode(*lPanel), lRegistry);
    REQUIRE(lPasted != nullptr);
    REQUIRE(ModifierAt(*lPasted, 0) != nullptr);
    CHECK(ModifierAt(*lPasted, 0)->Strength == doctest::Approx(5.f));
}

// =============================================================================
// Format version 2: an image's fields in its brush
// =============================================================================
TEST_CASE("UICanvasFile: a version 1 image reads into its brush; a key it lacks takes version 1's default")
{
    const WidgetClasses lRegistry;

    const OpaaxString lV1 = OpaaxString(R"({
    "Root": { "Type": "UIPanel", "Children": [
        { "Type": "UIImage", "Name": "Full", "Color": { "x": 0.1, "y": 0.2, "z": 0.3, "w": 0.4 },
          "Texture": "UI/Art.png", "Sheet": "UI/Icons.opaaxasset", "Frame": 5,
          "Border": { "Left": 1.0, "Right": 2.0, "Bottom": 3.0, "Top": 4.0 }, "BorderScale": 0.5,
          "Fill": "Horizontal", "FillAmount": 0.25 },
        { "Type": "UIImage", "Name": "Bare" }
    ] },
    "Version": 1
})");

    UICanvasFile::UICanvasDoc lDoc;
    REQUIRE(UICanvasFile::Deserialize(lV1, lRegistry, lDoc));
    REQUIRE(lDoc.Root->GetChildren().size() == 2u);

    const auto* lFull = dynamic_cast<const UIImage*>(lDoc.Root->GetChildren()[0].Get());
    REQUIRE(lFull != nullptr);
    CHECK(lFull->Brush.Tint.y == doctest::Approx(0.2f));
    CHECK(lFull->Brush.Tint.w == doctest::Approx(0.4f));
    CHECK(lFull->Brush.Texture.Path == OpaaxString("UI/Art.png"));
    CHECK(lFull->Brush.Sheet.Path == OpaaxString("UI/Icons.opaaxasset"));
    CHECK(lFull->Brush.Frame == 5);
    CHECK(lFull->Brush.Border.Bottom == doctest::Approx(3.f));
    CHECK(lFull->Brush.BorderScale == doctest::Approx(0.5f));
    CHECK(lFull->Fill == EUIFill::Horizontal);        // the fields outside the brush are untouched
    CHECK(lFull->FillAmount == doctest::Approx(0.25f));

    const auto* lBare = dynamic_cast<const UIImage*>(lDoc.Root->GetChildren()[1].Get());
    REQUIRE(lBare != nullptr);
    CHECK(lBare->Brush.Tint.x == doctest::Approx(1.f));   // version 1's white
    CHECK(lBare->Brush.Tint.w == doctest::Approx(1.f));
    CHECK_FALSE(lBare->Brush.HasImage());
    CHECK(lBare->Brush.Frame == -1);
    CHECK(lBare->Brush.BorderScale == doctest::Approx(1.f));

    // A file with no version at all is version 1 too.
    UICanvasFile::UICanvasDoc lUnversioned;
    REQUIRE(UICanvasFile::Deserialize(OpaaxString(R"({"Root":{"Type":"UIPanel","Children":[{"Type":"UIImage","Frame":3}]}})"),
                                      lRegistry, lUnversioned));
    CHECK(dynamic_cast<const UIImage*>(lUnversioned.Root->GetChildren()[0].Get())->Brush.Frame == 3);
}

TEST_CASE("UICanvasFile: version 2 is written, with the image's brush, and reads back the same")
{
    const WidgetClasses lRegistry;

    UICanvasFile::UICanvasDoc lDoc = UICanvasFile::MakeEmpty(1080.f);
    UIWidget* const lPanel = lDoc.Root->AddChild(NewObject<UIPanel>());
    auto lImage = NewObject<UIImage>();
    lImage->SetColor({ 0.5f, 0.6f, 0.7f, 0.8f });
    lImage->SetSheetFrame(OpaaxString("UI/Icons.opaaxasset"), 2);
    lImage->SetBorderScale(0.25f);
    lPanel->AddChild(Move(lImage));   // nested: the version reaches every node

    const OpaaxString lText = UICanvasFile::Serialize(lDoc);
    CHECK(lText.Find("\"Version\": 2") >= 0);
    CHECK(lText.Find("\"Brush\"") >= 0);
    CHECK(lText.Find("\"Color\"") < 0);

    UICanvasFile::UICanvasDoc lBack;
    REQUIRE(UICanvasFile::Deserialize(lText, lRegistry, lBack));
    const auto* lRead = dynamic_cast<const UIImage*>(lBack.Root->GetChildren()[0]->GetChildren()[0].Get());
    REQUIRE(lRead != nullptr);
    CHECK(lRead->Brush.Tint.z == doctest::Approx(0.7f));
    CHECK(lRead->Brush.Sheet.Path == OpaaxString("UI/Icons.opaaxasset"));
    CHECK(lRead->Brush.Frame == 2);
    CHECK(lRead->Brush.BorderScale == doctest::Approx(0.25f));

    // Written again, the same text: a version 2 file is not upgraded twice.
    CHECK(UICanvasFile::Serialize(lBack) == lText);
}

TEST_CASE("UICanvasFile: a version 3 file and a version 3 node are refused")
{
    const WidgetClasses lRegistry;

    UICanvasFile::UICanvasDoc lDoc;
    lDoc.ReferenceHeight = 999.f;
    CHECK_FALSE(UICanvasFile::Deserialize(OpaaxString(R"({"Version": 3, "Root": {"Type": "UIPanel"}})"),
                                          lRegistry, lDoc));
    CHECK(lDoc.ReferenceHeight == doctest::Approx(999.f));

    CHECK(UICanvasFile::DeserializeNode(OpaaxString(R"({"Version": 3, "Type": "UIImage"})"), lRegistry) == nullptr);
}

TEST_CASE("UICanvasFile: a node's text carries version 2; a pasted node without one upgrades as version 1")
{
    const WidgetClasses lRegistry;

    auto lImage = NewObject<UIImage>();
    lImage->SetColor({ 0.f, 1.f, 0.f, 1.f });
    const OpaaxString lText = UICanvasFile::SerializeNode(*lImage);
    CHECK(lText.Find("\"Version\": 2") >= 0);

    TStrongObjectPtr<UIWidget> lPasted = UICanvasFile::DeserializeNode(lText, lRegistry);
    REQUIRE(lPasted != nullptr);
    CHECK(static_cast<const UIImage*>(lPasted.Get())->Brush.Tint.y == doctest::Approx(1.f));

    // Copied by an older editor: no version, version 1 fields, an image under a panel.
    const OpaaxString lOld = OpaaxString(R"({ "Type": "UIPanel", "Name": "Group", "Children": [
        { "Type": "UIImage", "Name": "Icon", "Texture": "UI/Art.png", "Color": { "x": 0.0, "y": 0.0, "z": 1.0, "w": 1.0 } } ] })");
    TStrongObjectPtr<UIWidget> lOldPasted = UICanvasFile::DeserializeNode(lOld, lRegistry);
    REQUIRE(lOldPasted != nullptr);
    REQUIRE(lOldPasted->GetChildren().size() == 1u);
    const auto* lIcon = dynamic_cast<const UIImage*>(lOldPasted->GetChildren()[0].Get());
    REQUIRE(lIcon != nullptr);
    CHECK(lIcon->Brush.Texture.Path == OpaaxString("UI/Art.png"));
    CHECK(lIcon->Brush.Tint.z == doctest::Approx(1.f));
    CHECK(lIcon->Brush.Tint.x == doctest::Approx(0.f));
}
