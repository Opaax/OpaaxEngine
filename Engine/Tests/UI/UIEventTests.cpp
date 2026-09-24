// Suite: UI events (UI/UIEvents.h, UICanvas::RoutePointer / RouteKey) — bubbling, capture, focus.
//
// The contract is Slate's: the hit widget is asked first, then its parents, until one answers
// Handled; Enter/Leave are delivered to the hovered widget and never bubble; a press captures the
// pointer so the release reaches the presser wherever it lands. All of it pure.
#include <doctest.h>

#include "UI/UICanvas.h"
#include "UI/Widgets/UIImage.h"
#include "UI/Widgets/UIPanel.h"

using namespace Opaax;

namespace
{
    /** Records what it was asked and answers as told. */
    class Recorder final : public UIWidget
    {
    public:
        bool                        bHandlePointer = false;
        bool                        bHandleKey     = false;
        TDynArray<EUIPointerEventType> Pointer;
        TDynArray<bool>             Keys;   // bPressed per key event

        OpaaxStringID GetTypeName() const noexcept override { return OPAAX_ID("Recorder"); }

        EUIReply OnPointerEvent(const UIPointerEvent& InEvent) override
        {
            Pointer.emplace_back(InEvent.Type);
            return bHandlePointer ? EUIReply::Handled : EUIReply::Unhandled;
        }

        EUIReply OnKeyEvent(const UIKeyEvent& InEvent) override
        {
            Keys.emplace_back(InEvent.bPressed);
            return bHandleKey ? EUIReply::Handled : EUIReply::Unhandled;
        }

        Uint32 Count(const EUIPointerEventType InType) const
        {
            Uint32 lCount = 0;
            for (const EUIPointerEventType lType : Pointer) { if (lType == InType) { ++lCount; } }
            return lCount;
        }
    };

    UIRect Centred(const Vector2F& InSize, const Vector2F& InOffset = { 0.f, 0.f })
    {
        UIRect lRect;
        lRect.AnchoredPosition = InOffset;
        lRect.SizeDelta        = InSize;
        return lRect;
    }

    UIPointerEvent Ev(const EUIPointerEventType InType, const Vector2F& InAt,
                      const EUIPointerButton InButton = EUIPointerButton::Primary)
    {
        return UIPointerEvent{ InType, InAt, InButton };
    }

    struct Fixture
    {
        UICanvas  Canvas{ 1080.f };
        Recorder* Parent = nullptr;   // 400x400 at the origin
        Recorder* Child  = nullptr;   // 100x100 at the origin, inside Parent

        Fixture()
        {
            Canvas.SetTargetSize(1920, 1080);
            Parent = static_cast<Recorder*>(Canvas.Root().AddChild(MakeUnique<Recorder>()));
            Parent->SetRect(Centred({ 400.f, 400.f }));
            Child = static_cast<Recorder*>(Parent->AddChild(MakeUnique<Recorder>()));
            Child->SetRect(Centred({ 100.f, 100.f }));
            Canvas.Update();
        }
    };
}

TEST_CASE("UIEvents: a Down bubbles from the hit child to the parent that handles it")
{
    Fixture lF;
    lF.Parent->bHandlePointer = true;

    CHECK(lF.Canvas.RoutePointer(Ev(EUIPointerEventType::Down, { 0.f, 0.f })) == EUIReply::Handled);
    CHECK(lF.Child->Count(EUIPointerEventType::Down) == 1);    // asked first
    CHECK(lF.Parent->Count(EUIPointerEventType::Down) == 1);   // then the parent, which took it
    CHECK(lF.Canvas.GetPressed() == lF.Parent);                // the HANDLER captures, not the hit
}

TEST_CASE("UIEvents: nothing handling it, the event reaches the root and falls through")
{
    Fixture lF;

    CHECK(lF.Canvas.RoutePointer(Ev(EUIPointerEventType::Down, { 0.f, 0.f })) == EUIReply::Unhandled);
    CHECK(lF.Child->Count(EUIPointerEventType::Down) == 1);
    CHECK(lF.Parent->Count(EUIPointerEventType::Down) == 1);
    CHECK(lF.Canvas.GetPressed() == nullptr);

    // Off every widget: nobody is asked at all.
    CHECK(lF.Canvas.RoutePointer(Ev(EUIPointerEventType::Down, { 900.f, 500.f })) == EUIReply::Unhandled);
    CHECK(lF.Child->Count(EUIPointerEventType::Down) == 1);
}

TEST_CASE("UIEvents: a Move delivers Leave to the old hovered and Enter to the new, and is never handled")
{
    Fixture lF;
    lF.Child->bHandlePointer = true;   // handling Enter/Leave changes nothing about a Move's reply

    CHECK(lF.Canvas.RoutePointer(Ev(EUIPointerEventType::Move, { 0.f, 0.f }, EUIPointerButton::None)) == EUIReply::Unhandled);
    CHECK(lF.Canvas.GetHovered() == lF.Child);
    CHECK(lF.Child->Count(EUIPointerEventType::Enter) == 1);
    CHECK(lF.Parent->Count(EUIPointerEventType::Enter) == 0);   // delivered, not bubbled

    // Still over the child: no new Enter.
    lF.Canvas.RoutePointer(Ev(EUIPointerEventType::Move, { 10.f, 10.f }, EUIPointerButton::None));
    CHECK(lF.Child->Count(EUIPointerEventType::Enter) == 1);

    // Out to the parent's area: the child Leaves, the parent Enters.
    lF.Canvas.RoutePointer(Ev(EUIPointerEventType::Move, { 150.f, 150.f }, EUIPointerButton::None));
    CHECK(lF.Child->Count(EUIPointerEventType::Leave) == 1);
    CHECK(lF.Parent->Count(EUIPointerEventType::Enter) == 1);
    CHECK(lF.Canvas.GetHovered() == lF.Parent);

    // Off everything: the parent Leaves, nothing is hovered.
    lF.Canvas.RoutePointer(Ev(EUIPointerEventType::Move, { 900.f, 500.f }, EUIPointerButton::None));
    CHECK(lF.Parent->Count(EUIPointerEventType::Leave) == 1);
    CHECK(lF.Canvas.GetHovered() == nullptr);
}

TEST_CASE("UIEvents: the presser hears its Up even released outside, and capture ends")
{
    Fixture lF;
    lF.Child->bHandlePointer = true;

    lF.Canvas.RoutePointer(Ev(EUIPointerEventType::Down, { 0.f, 0.f }));
    REQUIRE(lF.Canvas.GetPressed() == lF.Child);

    // Released far outside every widget: the captured child is still the target.
    CHECK(lF.Canvas.RoutePointer(Ev(EUIPointerEventType::Up, { 900.f, 500.f })) == EUIReply::Handled);
    CHECK(lF.Child->Count(EUIPointerEventType::Up) == 1);
    CHECK(lF.Parent->Count(EUIPointerEventType::Up) == 0);
    CHECK(lF.Canvas.GetPressed() == nullptr);

    // With nothing captured, an Up goes to whatever is under it.
    CHECK(lF.Canvas.RoutePointer(Ev(EUIPointerEventType::Up, { 900.f, 500.f })) == EUIReply::Unhandled);
    CHECK(lF.Child->Count(EUIPointerEventType::Up) == 1);
}

TEST_CASE("UIEvents: a key bubbles from the focused widget; no focus, no delivery")
{
    Fixture lF;
    lF.Parent->bHandleKey = true;

    CHECK(lF.Canvas.RouteKey({ static_cast<EKeyCode>(1), true }) == EUIReply::Unhandled);
    CHECK(lF.Child->Keys.empty());

    lF.Canvas.SetFocus(lF.Child);
    CHECK(lF.Canvas.RouteKey({ static_cast<EKeyCode>(1), true }) == EUIReply::Handled);
    CHECK(lF.Child->Keys.size() == 1);    // asked first, declined
    CHECK(lF.Parent->Keys.size() == 1);   // took it

    lF.Canvas.SetFocus(nullptr);
    CHECK(lF.Canvas.RouteKey({ static_cast<EKeyCode>(1), false }) == EUIReply::Unhandled);
    CHECK(lF.Parent->Keys.size() == 1);
}

TEST_CASE("UIEvents: detaching a subtree forgets hovered, pressed and focused under it")
{
    Fixture lF;
    lF.Child->bHandlePointer = true;

    lF.Canvas.RoutePointer(Ev(EUIPointerEventType::Move, { 0.f, 0.f }, EUIPointerButton::None));
    lF.Canvas.RoutePointer(Ev(EUIPointerEventType::Down, { 0.f, 0.f }));
    lF.Canvas.SetFocus(lF.Child);
    REQUIRE(lF.Canvas.GetHovered() == lF.Child);
    REQUIRE(lF.Canvas.GetPressed() == lF.Child);
    REQUIRE(lF.Canvas.GetFocus()   == lF.Child);

    // Remove the PARENT: the child under it is gone with it, and nothing may still point at it.
    const TUniquePtr<UIWidget> lTaken = lF.Canvas.Root().RemoveChild(*lF.Parent);
    REQUIRE(lTaken != nullptr);
    CHECK(lF.Canvas.GetHovered() == nullptr);
    CHECK(lF.Canvas.GetPressed() == nullptr);
    CHECK(lF.Canvas.GetFocus()   == nullptr);
    CHECK(lTaken->GetCanvas()    == nullptr);
    CHECK(lF.Child->GetCanvas()  == nullptr);   // still alive under lTaken, detached with it

    // And the next Up with nothing captured is a plain miss, not a call into freed memory.
    CHECK(lF.Canvas.RoutePointer(Ev(EUIPointerEventType::Up, { 0.f, 0.f })) == EUIReply::Unhandled);
}

TEST_CASE("UIEvents: ClearPointer leaves the hovered widget and drops capture")
{
    Fixture lF;
    lF.Child->bHandlePointer = true;

    lF.Canvas.RoutePointer(Ev(EUIPointerEventType::Move, { 0.f, 0.f }, EUIPointerButton::None));
    lF.Canvas.RoutePointer(Ev(EUIPointerEventType::Down, { 0.f, 0.f }));

    lF.Canvas.ClearPointer();
    CHECK(lF.Child->Count(EUIPointerEventType::Leave) == 1);
    CHECK(lF.Canvas.GetHovered() == nullptr);
    CHECK(lF.Canvas.GetPressed() == nullptr);
}

TEST_CASE("UIEvents: a bare image or panel lets a click fall through — Unreal's SImage")
{
    UICanvas lCanvas(1080.f);
    lCanvas.SetTargetSize(1920, 1080);
    UIWidget* lPanel = lCanvas.Root().AddChild(MakeUnique<UIPanel>());
    lPanel->SetRect(Centred({ 400.f, 400.f }));
    UIWidget* lImage = lPanel->AddChild(MakeUnique<UIImage>());
    lImage->SetRect(Centred({ 100.f, 100.f }));
    lCanvas.Update();

    CHECK(lCanvas.HitTest({ 0.f, 0.f }) == lImage);   // it IS a hit target...
    CHECK(lCanvas.RoutePointer(Ev(EUIPointerEventType::Down, { 0.f, 0.f })) == EUIReply::Unhandled);   // ...that takes nothing
    CHECK(lCanvas.GetPressed() == nullptr);
}
