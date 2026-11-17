// Suite: the UI clock and transitions (UICanvas::Tick, UITransitionModifier). A transition eases to
// its state's pose over Duration, starts again from where it is when interrupted, gives the same pose at
// the same time whatever the frame rate, and sleeps when done: an idle canvas ticks nothing.
#include <doctest.h>

#include "UI/Modifiers/UITransitionModifier.h"
#include "UI/UICanvas.h"
#include "UI/Widgets/UIButton.h"
#include "UI/Widgets/UIImage.h"
#include "UI/Widgets/UIPanel.h"

using namespace Opaax;

namespace
{
    UIRect Centred(const Vector2F& InSize, const Vector2F& InOffset = { 0.f, 0.f })
    {
        UIRect lRect;
        lRect.AnchoredPosition = InOffset;
        lRect.SizeDelta        = InSize;
        return lRect;
    }

    UIPose Scaled(const float InScale)
    {
        UIPose lPose;
        lPose.Scale = { InScale, InScale };
        return lPose;
    }

    /** A button with a transition: hovered 1.1, pressed 0.9, disabled 0.5, over 0.1 s, linear. */
    struct Fixture
    {
        UICanvas              Canvas{ 1080.f };
        UIButton*             Button     = nullptr;
        UITransitionModifier* Transition = nullptr;

        Fixture()
        {
            Canvas.SetTargetSize(1920, 1080);
            Button = static_cast<UIButton*>(Canvas.Root().AddChild(NewObject<UIButton>()));
            Button->SetRect(Centred({ 200.f, 80.f }));

            Transition = &Button->AddModifier<UITransitionModifier>();
            Transition->Duration = 0.1f;
            Transition->Ease     = EEase::Linear;
            Transition->Hovered  = Scaled(1.1f);
            Transition->Pressed  = Scaled(0.9f);
            Transition->Disabled = Scaled(0.5f);
            Canvas.Update();
        }

        void Route(const EUIPointerEventType InType, const Vector2F& InAt)
        {
            Canvas.RoutePointer(UIPointerEvent{ InType, InAt, EUIPointerButton::Primary });
        }

        /** InSeconds in frames of 1/InHz, each ticked then drawn. */
        void Run(const float InSeconds, const float InHz = 60.f)
        {
            const Int32 lFrames = static_cast<Int32>(InSeconds * InHz + 0.5f);
            for (Int32 lFrame = 0; lFrame < lFrames; ++lFrame)
            {
                Canvas.Tick(1.f / InHz);
                Canvas.Update();
            }
        }

        float ScaleOf(const UIWidget& InWidget)
        {
            Canvas.Update();
            return InWidget.GetDrawTransform().Decompose().Scale.x;
        }

        float Scale() { return ScaleOf(*Button); }
    };
}

TEST_CASE("UITransition: Normal, Hovered, Pressed, Hovered, Normal, each eased over Duration")
{
    Fixture lF;
    CHECK(lF.Scale() == doctest::Approx(1.f));
    CHECK(lF.Transition->GetState() == EUIInteractionState::Normal);

    lF.Route(EUIPointerEventType::Move, { 0.f, 0.f });
    CHECK(lF.Button->GetInteractionState() == EUIInteractionState::Hovered);
    CHECK(lF.Transition->IsPlaying());

    lF.Run(0.05f);   // halfway, linear
    CHECK(lF.Scale() == doctest::Approx(1.05f).epsilon(1e-3));
    lF.Run(0.1f);
    CHECK(lF.Scale() == doctest::Approx(1.1f));

    lF.Route(EUIPointerEventType::Down, { 0.f, 0.f });
    CHECK(lF.Button->GetInteractionState() == EUIInteractionState::Pressed);
    lF.Run(0.2f);
    CHECK(lF.Scale() == doctest::Approx(0.9f));

    lF.Route(EUIPointerEventType::Up, { 0.f, 0.f });
    lF.Run(0.2f);
    CHECK(lF.Scale() == doctest::Approx(1.1f));

    lF.Route(EUIPointerEventType::Move, { 900.f, 500.f });
    CHECK(lF.Button->GetInteractionState() == EUIInteractionState::Normal);
    lF.Run(0.2f);
    CHECK(lF.Scale() == doctest::Approx(1.f));
}

TEST_CASE("UITransition: interrupted halfway, it goes back from where it is, without a jump")
{
    Fixture lF;
    lF.Route(EUIPointerEventType::Move, { 0.f, 0.f });
    lF.Run(0.05f);
    const float lHalfway = lF.Scale();
    CHECK(lHalfway == doctest::Approx(1.05f).epsilon(1e-3));

    // Leaving now: the next frame starts at the pose shown, not at 1.1.
    lF.Route(EUIPointerEventType::Move, { 900.f, 500.f });
    CHECK(lF.Scale() == doctest::Approx(lHalfway));
    lF.Run(1.f / 60.f);
    CHECK(lF.Scale() < lHalfway);
    CHECK(lF.Scale() > 1.f);

    lF.Run(0.2f);
    CHECK(lF.Scale() == doctest::Approx(1.f));
}

TEST_CASE("UITransition: the same pose at the same time at 60 and 240 Hz")
{
    const EEase lEases[] = { EEase::QuadOut, EEase::BackOut };
    for (const EEase lEase : lEases)
    {
        Fixture l60;
        Fixture l240;
        l60.Transition->Ease  = lEase;
        l240.Transition->Ease = lEase;

        l60.Route(EUIPointerEventType::Move, { 0.f, 0.f });
        l240.Route(EUIPointerEventType::Move, { 0.f, 0.f });

        l60.Run(0.05f, 60.f);
        l240.Run(0.05f, 240.f);
        CHECK(l60.Scale() == doctest::Approx(l240.Scale()).epsilon(1e-4));
    }
}

TEST_CASE("UITransition: Disabled wins over hovered and pressed; a button loaded disabled starts there")
{
    Fixture lF;
    lF.Route(EUIPointerEventType::Move, { 0.f, 0.f });
    lF.Run(0.2f);

    lF.Button->SetEnabled(false);
    CHECK(lF.Button->GetInteractionState() == EUIInteractionState::Disabled);
    lF.Run(0.2f);
    CHECK(lF.Scale() == doctest::Approx(0.5f));

    lF.Button->SetEnabled(true);
    CHECK(lF.Button->GetInteractionState() == EUIInteractionState::Hovered);
    lF.Run(0.2f);
    CHECK(lF.Scale() == doctest::Approx(1.1f));

    // Added to a button that is disabled already: Disabled at once, nothing to play.
    UIButton* lOther = static_cast<UIButton*>(lF.Canvas.Root().AddChild(NewObject<UIButton>()));
    lOther->SetRect(Centred({ 50.f, 50.f }, { 400.f, 0.f }));
    lOther->SetEnabled(false);
    UITransitionModifier& lLate = lOther->AddModifier<UITransitionModifier>();
    lLate.Disabled = Scaled(0.5f);
    lLate.PostEditChange();
    CHECK(lLate.GetState() == EUIInteractionState::Disabled);
    CHECK_FALSE(lLate.IsPlaying());
    CHECK(lF.ScaleOf(*lOther) == doctest::Approx(0.5f));
}

TEST_CASE("UITransition: a panel is hovered while a widget in it is")
{
    UICanvas lCanvas{ 1080.f };
    lCanvas.SetTargetSize(1920, 1080);

    UIWidget* lPanel = lCanvas.Root().AddChild(NewObject<UIPanel>());
    lPanel->SetRect(Centred({ 400.f, 400.f }));
    UIWidget* lImage = lPanel->AddChild(NewObject<UIImage>());
    lImage->SetRect(Centred({ 100.f, 100.f }));

    UITransitionModifier& lTransition = lPanel->AddModifier<UITransitionModifier>();
    lTransition.Hovered = Scaled(1.2f);
    lCanvas.Update();

    lCanvas.RoutePointer(UIPointerEvent{ EUIPointerEventType::Move, { 0.f, 0.f } });
    CHECK(lCanvas.GetHovered() == lImage);
    CHECK(lPanel->GetInteractionState() == EUIInteractionState::Hovered);
    CHECK(lTransition.GetState() == EUIInteractionState::Hovered);

    // Over the panel's bare area: it takes no hits, so nothing under the pointer is in it.
    lCanvas.RoutePointer(UIPointerEvent{ EUIPointerEventType::Move, { 150.f, 150.f } });
    CHECK(lPanel->GetInteractionState() == EUIInteractionState::Normal);
}

TEST_CASE("UITransition: it sleeps at the end, and an idle canvas ticks nothing")
{
    Fixture lF;
    CHECK(lF.Canvas.GetAwakeCount() == 0u);

    lF.Route(EUIPointerEventType::Move, { 0.f, 0.f });
    CHECK(lF.Canvas.GetAwakeCount() == 1u);

    lF.Run(0.2f);
    CHECK(lF.Canvas.GetAwakeCount() == 0u);
    CHECK_FALSE(lF.Transition->IsPlaying());

    // A move over the same widget changes no state: nothing wakes.
    lF.Route(EUIPointerEventType::Move, { 10.f, 0.f });
    CHECK(lF.Canvas.GetAwakeCount() == 0u);

    // Duration 0: the pose is there at once, nothing ticks.
    lF.Transition->Duration = 0.f;
    lF.Route(EUIPointerEventType::Move, { 900.f, 500.f });
    CHECK(lF.Canvas.GetAwakeCount() == 0u);
    CHECK(lF.Scale() == doctest::Approx(1.f));
}

TEST_CASE("UITransition: a widget removed while it plays leaves nothing awake")
{
    Fixture lF;
    lF.Route(EUIPointerEventType::Move, { 0.f, 0.f });
    REQUIRE(lF.Canvas.GetAwakeCount() == 1u);

    TStrongObjectPtr<UIWidget> lRemoved = lF.Canvas.Root().RemoveChild(*lF.Button);
    CHECK(lF.Canvas.GetAwakeCount() == 0u);

    // Still awake by itself: it ticks again in the canvas it joins.
    UICanvas lOther{ 1080.f };
    lOther.SetTargetSize(1920, 1080);
    lOther.Root().AddChild(Move(lRemoved));
    CHECK(lOther.GetAwakeCount() == 1u);

    // Removed and destroyed there: nothing is left to tick, and a Tick is safe.
    lOther.Root().RemoveChild(*lF.Button);
    CHECK(lOther.GetAwakeCount() == 0u);
    lOther.Tick(0.016f);
    CHECK(lOther.GetAwakeCount() == 0u);
}
