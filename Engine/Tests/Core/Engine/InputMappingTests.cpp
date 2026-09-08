// Suite: input mapping — modifiers, evaluation, priority/consumption, and binding (⑦-B B1).
//
// WHY THIS EXISTS.
//   Gameplay reads OPAAX_ID("Jump"), not EKeyCode::Space. Everything between those two is pure:
//   a modifier pipeline over Vector2F, a priority-sorted context stack, four trigger phases, and
//   a delegate table. None of it needs a window, a world or a GL context, so all of it is gated
//   here rather than by a screenshot.
//
//   The evaluator takes a REAL InputManager (InputManagerTests proves it constructs headlessly)
//   and is driven through its own feed — OnKeyPressed / OnKeyReleased / EndFrame. No fake input
//   source was invented for testability: an instrument that shares no code with the thing it
//   measures is the point of [[L21]], and here the real feed IS available.
#include <cmath>

#include <doctest.h>

#include "Application/Services/IPaths.h"
#include "Core/String/OpaaxStringID.hpp"
#include "Engine/Config/EngineConfigData.h"
#include "Engine/GameInstance/GameInstanceContext.h"
#include "Engine/Input/InputActionEvaluator.h"
#include "Engine/Input/InputMappingSubsystem.h"
#include "Engine/Input/InputModifiers.h"
#include "Engine/Input/InputTypes.h"
#include "Engine/Subsystems/EventBus/EngineEventBus.h"
#include "Engine/Subsystems/Input/InputManager.h"
#include "Engine/Subsystems/Resources/ResourceManager.h"
#include "World/WorldManager.h"

using namespace Opaax;

namespace
{
    OpaaxStringID Name(const char* InText) { return OpaaxStringID(OpaaxString(InText)); }

    InputModifierData Negate()  { InputModifierData m; m.Type = EInputModifier::Negate;    return m; }
    InputModifierData Swizzle() { InputModifierData m; m.Type = EInputModifier::Swizzle;   return m; }
    InputModifierData Norm()    { InputModifierData m; m.Type = EInputModifier::Normalize; return m; }

    InputModifierData Scalar(float InX, float InY)
    {
        InputModifierData m;
        m.Type  = EInputModifier::Scalar;
        m.Scale = Vector2F{InX, InY};
        return m;
    }

    InputModifierData DeadZone(float InLower, float InUpper)
    {
        InputModifierData m;
        m.Type          = EInputModifier::DeadZone;
        m.DeadZoneLower = InLower;
        m.DeadZoneUpper = InUpper;
        return m;
    }

    InputAction MakeAction(const char* InName, EInputValueType InType, float InHold = 0.4f)
    {
        InputAction lAction;
        lAction.Name        = Name(InName);
        lAction.ValueType   = InType;
        lAction.HoldSeconds = InHold;
        return lAction;
    }

    InputKeyBinding Bind(const char* InAction, EKeyCode InKey, bool bInConsume = true)
    {
        InputKeyBinding lBinding;
        lBinding.Action   = Name(InAction);
        lBinding.Key      = InKey;
        lBinding.bConsume = bInConsume;
        return lBinding;
    }

    /** Advance one whole frame: the readers run, then the frame boundary closes (IN2). */
    void Step(InputActionEvaluator& InEval, InputManager& InInput, double InDelta = 1.0 / 60.0)
    {
        InEval.Evaluate(InInput, InDelta);
        InInput.EndFrame();
    }

    /** The engine-side half of a GameInstanceContext, owned by the test. */
    struct ContextFixture
    {
        WorldManager     Worlds;
        ResourceManager  Resources;
        EngineEventBus   Events;
        InputManager     Input;
        EngineConfigData Config;

        GameInstanceContext Make()
        {
            return GameInstanceContext{Worlds, Resources, IPaths::Null(), Events, Input, Config};
        }
    };
}

// =============================================================================
TEST_SUITE("InputModifiers")
{
    TEST_CASE("Negate, Swizzle and Scalar")
    {
        CHECK(InputModifiers::Apply(Vector2F{1.f, 2.f}, Negate()).x == doctest::Approx(-1.f));
        CHECK(InputModifiers::Apply(Vector2F{1.f, 2.f}, Negate()).y == doctest::Approx(-2.f));

        // A SWAP, so applying it twice is the identity — which is what lets one modifier serve
        // both halves of a vertical WASD pair.
        const Vector2F lSwapped = InputModifiers::Apply(Vector2F{1.f, 0.f}, Swizzle());
        CHECK(lSwapped.x == doctest::Approx(0.f));
        CHECK(lSwapped.y == doctest::Approx(1.f));
        CHECK(InputModifiers::Apply(lSwapped, Swizzle()).x == doctest::Approx(1.f));

        const Vector2F lScaled = InputModifiers::Apply(Vector2F{2.f, 3.f}, Scalar(0.5f, 2.f));
        CHECK(lScaled.x == doctest::Approx(1.f));
        CHECK(lScaled.y == doctest::Approx(6.f));
    }

    TEST_CASE("Normalize shrinks a diagonal and leaves a short vector alone")
    {
        // The WASD diagonal: without this, moving up-right is 1.41x as fast as moving right.
        const Vector2F lDiagonal = InputModifiers::Apply(Vector2F{1.f, 1.f}, Norm());
        const float    lLength   = std::sqrt(lDiagonal.x * lDiagonal.x + lDiagonal.y * lDiagonal.y);
        CHECK(lLength == doctest::Approx(1.f));

        // Shrink ONLY. Growing a half-pressed stick to full would be the opposite of the point.
        const Vector2F lShort = InputModifiers::Apply(Vector2F{0.3f, 0.f}, Norm());
        CHECK(lShort.x == doctest::Approx(0.3f));

        const Vector2F lZero = InputModifiers::Apply(Vector2F{0.f, 0.f}, Norm());
        CHECK(lZero.x == doctest::Approx(0.f));
        CHECK(lZero.y == doctest::Approx(0.f));
    }

    TEST_CASE("DeadZone: inside, boundary, rescaled, and above upper")
    {
        const InputModifierData lZone = DeadZone(0.2f, 1.0f);

        CHECK(InputModifiers::Apply(Vector2F{0.1f, 0.f}, lZone).x == doctest::Approx(0.f));
        CHECK(InputModifiers::Apply(Vector2F{0.2f, 0.f}, lZone).x == doctest::Approx(0.f));

        // Rescaled across the live band, not merely passed through: 0.6 sits halfway between
        // 0.2 and 1.0, so it must read 0.5 — a version that only clipped would read 0.6.
        CHECK(InputModifiers::Apply(Vector2F{0.6f, 0.f}, lZone).x == doctest::Approx(0.5f));

        CHECK(InputModifiers::Apply(Vector2F{1.f, 0.f}, lZone).x == doctest::Approx(1.f));
        CHECK(InputModifiers::Apply(Vector2F{0.f, 0.f}, lZone).x == doctest::Approx(0.f));
    }

    TEST_CASE("DeadZone is RADIAL, so a diagonal cannot escape a zone neither axis could")
    {
        const InputModifierData lZone = DeadZone(0.5f, 1.0f);

        // Per component, (0.4, 0.4) would be killed on both axes. Radially its magnitude is
        // 0.57, which is outside the zone — and that is the correct answer for a stick.
        const Vector2F lOut = InputModifiers::Apply(Vector2F{0.4f, 0.4f}, lZone);
        CHECK(lOut.x > 0.f);
        CHECK(lOut.y > 0.f);
    }

    TEST_CASE("ApplyAll runs IN ORDER, and the order changes the answer")
    {
        const TDynArray<InputModifierData> lZoneThenScale{DeadZone(0.2f, 1.f), Scalar(10.f, 1.f)};
        const TDynArray<InputModifierData> lScaleThenZone{Scalar(10.f, 1.f), DeadZone(0.2f, 1.f)};

        // 0.1 is inside the dead zone, so zone-first kills it and scaling 0 stays 0. Scale-first
        // lifts it to 1.0, which is outside. This is why the modifier list is ordered.
        CHECK(InputModifiers::ApplyAll(Vector2F{0.1f, 0.f}, lZoneThenScale).x == doctest::Approx(0.f));
        CHECK(InputModifiers::ApplyAll(Vector2F{0.1f, 0.f}, lScaleThenZone).x > 0.f);
    }
}

// =============================================================================
TEST_SUITE("InputActionEvaluator — actions and contexts")
{
    TEST_CASE("RegisterAction refuses an empty or duplicate name")
    {
        InputActionEvaluator lEval;

        CHECK_FALSE(lEval.RegisterAction(MakeAction("", EInputValueType::Bool)));
        CHECK(lEval.RegisterAction(MakeAction("Jump", EInputValueType::Bool)));
        CHECK_FALSE(lEval.RegisterAction(MakeAction("Jump", EInputValueType::Axis1D)));
        CHECK(lEval.GetActionCount() == 1);

        REQUIRE(lEval.FindAction(Name("Jump")) != nullptr);
        CHECK(lEval.FindAction(Name("Jump"))->ValueType == EInputValueType::Bool);
        CHECK(lEval.FindAction(Name("Nope")) == nullptr);
    }

    TEST_CASE("Contexts sort by priority, highest first")
    {
        InputActionEvaluator lEval;
        lEval.RegisterAction(MakeAction("Jump", EInputValueType::Bool));

        InputMappingContext lLow;
        lLow.Name     = Name("Gameplay");
        lLow.Priority = 0;
        lLow.Bindings.emplace_back(Bind("Jump", EKeyCode::Space));

        InputMappingContext lHigh;
        lHigh.Name     = Name("Menu");
        lHigh.Priority = 100;
        lHigh.Bindings.emplace_back(Bind("Jump", EKeyCode::Space));

        CHECK(lEval.AddContext(lLow));
        CHECK(lEval.AddContext(lHigh));
        CHECK(lEval.GetContextCount() == 2);

        REQUIRE(lEval.GetContextAt(0) != nullptr);
        CHECK(lEval.GetContextAt(0)->Name == Name("Menu"));
        CHECK(lEval.GetContextAt(1)->Name == Name("Gameplay"));

        // IDEMPOTENT: true because the postcondition holds, and the stack must NOT grow — a level
        // swap has two Play worlds adding the same context, and doubling it would double every
        // binding's contribution to its action.
        CHECK(lEval.AddContext(lHigh));
        CHECK(lEval.GetContextCount() == 2);

        CHECK(lEval.RemoveContext(Name("Menu")));
        CHECK(lEval.GetContextCount() == 1);
        CHECK_FALSE(lEval.RemoveContext(Name("Menu")));
    }

    TEST_CASE("Re-adding a context does not double its contribution")
    {
        InputActionEvaluator lEval;
        InputManager         lInput;
        lEval.RegisterAction(MakeAction("Move", EInputValueType::Axis1D));

        InputMappingContext lContext;
        lContext.Name = Name("Gameplay");
        lContext.Bindings.emplace_back(Bind("Move", EKeyCode::D));

        CHECK(lEval.AddContext(lContext));
        CHECK(lEval.AddContext(lContext));   // the second Play world during a level swap

        lInput.OnKeyPressed(EKeyCode::D, false);
        Step(lEval, lInput);

        // 1, not 2. This is the assertion the idempotence is FOR — a doubled stack would read 2
        // and the character would walk at twice the speed after every level change.
        CHECK(lEval.GetValue(Name("Move")).AsAxis1D() == doctest::Approx(1.f));
    }

    TEST_CASE("A binding naming an unregistered action is SKIPPED, not silently dead")
    {
        InputActionEvaluator lEval;
        lEval.RegisterAction(MakeAction("Jump", EInputValueType::Bool));

        InputMappingContext lContext;
        lContext.Name = Name("Gameplay");
        lContext.Bindings.emplace_back(Bind("Jump", EKeyCode::Space));
        lContext.Bindings.emplace_back(Bind("Typo", EKeyCode::E));

        CHECK(lEval.AddContext(lContext));
        REQUIRE(lEval.GetContextAt(0) != nullptr);
        CHECK(lEval.GetContextAt(0)->Bindings.size() == 1);
    }

    TEST_CASE("A GAMEPAD binding is refused, because IN7 means it has no feed")
    {
        InputActionEvaluator lEval;
        lEval.RegisterAction(MakeAction("Move", EInputValueType::Axis2D));

        InputMappingContext lContext;
        lContext.Name = Name("Gameplay");
        lContext.Bindings.emplace_back(Bind("Move", EKeyCode::A));
        lContext.Bindings.emplace_back(Bind("Move", EKeyCode::Gamepad_LeftX));

        // Accepted-and-never-firing is the failure mode this engine refuses; the code range is
        // reserved but nothing polls it, so the binding is dropped with a warning.
        CHECK(lEval.AddContext(lContext));
        REQUIRE(lEval.GetContextAt(0) != nullptr);
        CHECK(lEval.GetContextAt(0)->Bindings.size() == 1);
    }
}

// =============================================================================
TEST_SUITE("InputActionEvaluator — triggers")
{
    TEST_CASE("Started fires once, Triggered every frame, Completed on release")
    {
        InputActionEvaluator lEval;
        InputManager         lInput;
        lEval.RegisterAction(MakeAction("Jump", EInputValueType::Bool));

        InputMappingContext lContext;
        lContext.Name = Name("Gameplay");
        lContext.Bindings.emplace_back(Bind("Jump", EKeyCode::Space));
        lEval.AddContext(lContext);

        lInput.OnKeyPressed(EKeyCode::Space, false);
        Step(lEval, lInput);

        const InputActionState* lState = lEval.FindState(Name("Jump"));
        REQUIRE(lState != nullptr);
        CHECK(lState->bStarted);
        CHECK(lState->bTriggered);
        CHECK_FALSE(lState->bCompleted);

        Step(lEval, lInput);                    // still held
        CHECK_FALSE(lState->bStarted);
        CHECK(lState->bTriggered);

        lInput.OnKeyReleased(EKeyCode::Space);
        Step(lEval, lInput);
        CHECK_FALSE(lState->bTriggered);
        CHECK(lState->bCompleted);

        Step(lEval, lInput);                    // idle
        CHECK_FALSE(lState->bCompleted);
    }

    TEST_CASE("A press and release inside ONE frame is not dropped (IN3)")
    {
        InputActionEvaluator lEval;
        InputManager         lInput;
        lEval.RegisterAction(MakeAction("Jump", EInputValueType::Bool));

        InputMappingContext lContext;
        lContext.Name = Name("Gameplay");
        lContext.Bindings.emplace_back(Bind("Jump", EKeyCode::Space));
        lEval.AddContext(lContext);

        // Both edges land before a single Evaluate — IsKeyDown is already false. A version that
        // read only the held state would lose the tap entirely, which is what IN3 exists for.
        lInput.OnKeyPressed(EKeyCode::Space, false);
        lInput.OnKeyReleased(EKeyCode::Space);

        Step(lEval, lInput);

        const InputActionState* lState = lEval.FindState(Name("Jump"));
        REQUIRE(lState != nullptr);
        CHECK(lState->bStarted);
        CHECK(lState->bTriggered);

        Step(lEval, lInput);
        CHECK(lState->bCompleted);
    }

    TEST_CASE("Hold fires ONCE at the threshold and re-arms only after release")
    {
        InputActionEvaluator lEval;
        InputManager         lInput;
        lEval.RegisterAction(MakeAction("Crouch", EInputValueType::Bool, 0.25f));

        InputMappingContext lContext;
        lContext.Name = Name("Gameplay");
        lContext.Bindings.emplace_back(Bind("Crouch", EKeyCode::C));
        lEval.AddContext(lContext);

        const InputActionState* lState = nullptr;

        lInput.OnKeyPressed(EKeyCode::C, false);

        Step(lEval, lInput, 0.1);
        lState = lEval.FindState(Name("Crouch"));
        REQUIRE(lState != nullptr);
        CHECK_FALSE(lState->bHold);

        Step(lEval, lInput, 0.1);
        CHECK_FALSE(lState->bHold);

        Step(lEval, lInput, 0.1);               // 0.30 >= 0.25
        CHECK(lState->bHold);

        // ONCE. A latch that only set bHold would re-fire every frame the key stays down.
        Step(lEval, lInput, 0.1);
        CHECK_FALSE(lState->bHold);
        CHECK(lState->bTriggered);

        lInput.OnKeyReleased(EKeyCode::C);
        Step(lEval, lInput, 0.1);
        CHECK(lState->HeldSeconds == doctest::Approx(0.f));

        lInput.OnKeyPressed(EKeyCode::C, false);
        Step(lEval, lInput, 0.3);
        CHECK(lState->bHold);
    }
}

// =============================================================================
TEST_SUITE("InputActionEvaluator — composites, priority and consumption")
{
    TEST_CASE("Four keys and two modifiers make an Axis2D, and the diagonal is not faster")
    {
        InputActionEvaluator lEval;
        InputManager         lInput;

        // Normalize on the ACTION, not on the bindings — see the diagonal check below.
        InputAction lMove = MakeAction("Move", EInputValueType::Axis2D);
        lMove.Modifiers.emplace_back(Norm());
        lEval.RegisterAction(lMove);

        // The Unreal composite, with no composite concept in the format: raw rides in x, and
        // Negate/Swizzle move it where it belongs.
        InputKeyBinding lRight = Bind("Move", EKeyCode::D);
        InputKeyBinding lLeft  = Bind("Move", EKeyCode::A);
        lLeft.Modifiers.emplace_back(Negate());
        InputKeyBinding lUp    = Bind("Move", EKeyCode::W);
        lUp.Modifiers.emplace_back(Swizzle());
        InputKeyBinding lDown  = Bind("Move", EKeyCode::S);
        lDown.Modifiers.emplace_back(Negate());
        lDown.Modifiers.emplace_back(Swizzle());

        InputMappingContext lContext;
        lContext.Name = Name("Gameplay");
        lContext.Bindings.emplace_back(lRight);
        lContext.Bindings.emplace_back(lLeft);
        lContext.Bindings.emplace_back(lUp);
        lContext.Bindings.emplace_back(lDown);
        lEval.AddContext(lContext);

        lInput.OnKeyPressed(EKeyCode::D, false);
        Step(lEval, lInput);
        CHECK(lEval.GetValue(Name("Move")).AsAxis2D().x == doctest::Approx(1.f));
        CHECK(lEval.GetValue(Name("Move")).AsAxis2D().y == doctest::Approx(0.f));

        lInput.OnKeyPressed(EKeyCode::W, false);
        Step(lEval, lInput);

        // THE DIAGONAL. Each binding contributes a unit vector, so a Normalize on the BINDINGS
        // would change nothing — only the action-level modifier can clamp the sum. Without it
        // this reads (1, 1), magnitude 1.41, and the player moves faster diagonally.
        const Vector2F lDiagonal = lEval.GetValue(Name("Move")).AsAxis2D();
        const float    lLength   = std::sqrt(lDiagonal.x * lDiagonal.x + lDiagonal.y * lDiagonal.y);
        CHECK(lLength == doctest::Approx(1.f));
        CHECK(lDiagonal.x == doctest::Approx(lDiagonal.y));

        // Opposite keys cancel, which is the accumulate-then-read model working.
        lInput.OnKeyPressed(EKeyCode::A, false);
        Step(lEval, lInput);
        CHECK(lEval.GetValue(Name("Move")).AsAxis2D().x == doctest::Approx(0.f));

        CHECK(lEval.GetValue(Name("Move")).Type == EInputValueType::Axis2D);
    }

    TEST_CASE("A higher-priority context CONSUMES the key, so the lower one never fires")
    {
        InputActionEvaluator lEval;
        InputManager         lInput;
        lEval.RegisterAction(MakeAction("Jump", EInputValueType::Bool));
        lEval.RegisterAction(MakeAction("MenuAccept", EInputValueType::Bool));

        InputMappingContext lGameplay;
        lGameplay.Name     = Name("Gameplay");
        lGameplay.Priority = 0;
        lGameplay.Bindings.emplace_back(Bind("Jump", EKeyCode::Space));
        lEval.AddContext(lGameplay);

        lInput.OnKeyPressed(EKeyCode::Space, false);
        Step(lEval, lInput);
        CHECK(lEval.GetValue(Name("Jump")).AsBool());

        InputMappingContext lMenu;
        lMenu.Name     = Name("Menu");
        lMenu.Priority = 100;
        lMenu.Bindings.emplace_back(Bind("MenuAccept", EKeyCode::Space));
        lEval.AddContext(lMenu);

        // The menu SWALLOWS Space — Jump does not merely rank lower, it stops happening. This is
        // the property the runtime HUD needs, and it is per KEY, not per action.
        Step(lEval, lInput);
        CHECK(lEval.GetValue(Name("MenuAccept")).AsBool());
        CHECK_FALSE(lEval.GetValue(Name("Jump")).AsBool());

        // Popping the menu restores it, with nothing falsely held.
        lEval.RemoveContext(Name("Menu"));
        Step(lEval, lInput);
        CHECK(lEval.GetValue(Name("Jump")).AsBool());
        CHECK_FALSE(lEval.GetValue(Name("MenuAccept")).AsBool());
    }

    TEST_CASE("bConsume = false lets BOTH contexts see the key")
    {
        InputActionEvaluator lEval;
        InputManager         lInput;
        lEval.RegisterAction(MakeAction("Jump", EInputValueType::Bool));
        lEval.RegisterAction(MakeAction("Screenshot", EInputValueType::Bool));

        InputMappingContext lOverlay;
        lOverlay.Name     = Name("Overlay");
        lOverlay.Priority = 100;
        lOverlay.Bindings.emplace_back(Bind("Screenshot", EKeyCode::Space, /*bConsume*/ false));
        lEval.AddContext(lOverlay);

        InputMappingContext lGameplay;
        lGameplay.Name     = Name("Gameplay");
        lGameplay.Priority = 0;
        lGameplay.Bindings.emplace_back(Bind("Jump", EKeyCode::Space));
        lEval.AddContext(lGameplay);

        lInput.OnKeyPressed(EKeyCode::Space, false);
        Step(lEval, lInput);

        CHECK(lEval.GetValue(Name("Screenshot")).AsBool());
        CHECK(lEval.GetValue(Name("Jump")).AsBool());
    }

    TEST_CASE("Consumption is per KEY — an unrelated key in the same context still fires")
    {
        InputActionEvaluator lEval;
        InputManager         lInput;
        lEval.RegisterAction(MakeAction("Jump", EInputValueType::Bool));
        lEval.RegisterAction(MakeAction("Fire", EInputValueType::Bool));
        lEval.RegisterAction(MakeAction("MenuAccept", EInputValueType::Bool));

        InputMappingContext lMenu;
        lMenu.Name     = Name("Menu");
        lMenu.Priority = 100;
        lMenu.Bindings.emplace_back(Bind("MenuAccept", EKeyCode::Space));
        lEval.AddContext(lMenu);

        InputMappingContext lGameplay;
        lGameplay.Name     = Name("Gameplay");
        lGameplay.Priority = 0;
        lGameplay.Bindings.emplace_back(Bind("Jump", EKeyCode::Space));
        lGameplay.Bindings.emplace_back(Bind("Fire", EKeyCode::F));
        lEval.AddContext(lGameplay);

        lInput.OnKeyPressed(EKeyCode::Space, false);
        lInput.OnKeyPressed(EKeyCode::F, false);
        Step(lEval, lInput);

        CHECK_FALSE(lEval.GetValue(Name("Jump")).AsBool());   // Space was swallowed
        CHECK(lEval.GetValue(Name("Fire")).AsBool());         // F was not
    }
}

// =============================================================================
TEST_SUITE("InputMappingSubsystem — binding")
{
    namespace
    {
        /** A bound listener, standing in for a world subsystem. */
        class Listener
        {
        public:
            void OnStarted(const InputActionValue&)   { ++Started; }
            void OnTriggered(const InputActionValue& InValue) { ++Triggered; Last = InValue; }
            void OnCompleted(const InputActionValue&) { ++Completed; }

            int Started   = 0;
            int Triggered = 0;
            int Completed = 0;

            InputActionValue Last;
        };
    }

    TEST_CASE("Bind fires the right handler for the right phase")
    {
        ContextFixture         lFixture;
        GameInstanceContext    lContext = lFixture.Make();
        InputMappingSubsystem  lInput(lContext);
        Listener               lListener;

        lInput.RegisterAction(MakeAction("Jump", EInputValueType::Bool));

        InputMappingContext lMapping;
        lMapping.Name = Name("Gameplay");
        lMapping.Bindings.emplace_back(Bind("Jump", EKeyCode::Space));
        REQUIRE(lInput.AddContext(lMapping));

        lInput.Bind(Name("Jump"), EInputTrigger::Started,   &lListener, &Listener::OnStarted);
        lInput.Bind(Name("Jump"), EInputTrigger::Triggered, &lListener, &Listener::OnTriggered);
        lInput.Bind(Name("Jump"), EInputTrigger::Completed, &lListener, &Listener::OnCompleted);
        CHECK(lInput.GetBindingCount() == 3);

        lFixture.Input.OnKeyPressed(EKeyCode::Space, false);
        lInput.Update(1.0 / 60.0);
        lFixture.Input.EndFrame();

        CHECK(lListener.Started == 1);
        CHECK(lListener.Triggered == 1);
        CHECK(lListener.Completed == 0);
        CHECK(lListener.Last.AsBool());

        lInput.Update(1.0 / 60.0);
        lFixture.Input.EndFrame();
        CHECK(lListener.Started == 1);      // still once
        CHECK(lListener.Triggered == 2);    // every frame

        lFixture.Input.OnKeyReleased(EKeyCode::Space);
        lInput.Update(1.0 / 60.0);
        lFixture.Input.EndFrame();
        CHECK(lListener.Completed == 1);
        CHECK(lListener.Triggered == 2);
    }

    TEST_CASE("UnbindAll(this) removes every registration an owner made")
    {
        ContextFixture        lFixture;
        GameInstanceContext   lContext = lFixture.Make();
        InputMappingSubsystem lInput(lContext);
        Listener              lOwnerA;
        Listener              lOwnerB;

        lInput.RegisterAction(MakeAction("Jump", EInputValueType::Bool));

        InputMappingContext lMapping;
        lMapping.Name = Name("Gameplay");
        lMapping.Bindings.emplace_back(Bind("Jump", EKeyCode::Space));
        lInput.AddContext(lMapping);

        lInput.Bind(Name("Jump"), EInputTrigger::Started,   &lOwnerA, &Listener::OnStarted);
        lInput.Bind(Name("Jump"), EInputTrigger::Triggered, &lOwnerA, &Listener::OnTriggered);
        lInput.Bind(Name("Jump"), EInputTrigger::Started,   &lOwnerB, &Listener::OnStarted);
        CHECK(lInput.GetBindingCount() == 3);

        // THE LIFETIME CONTRACT: one call in the owner's Shutdown. Without it a destroyed world
        // subsystem stays in this list and the next Broadcast calls into freed memory.
        CHECK(lInput.UnbindAll(&lOwnerA) == 2);
        CHECK(lInput.GetBindingCount() == 1);

        lFixture.Input.OnKeyPressed(EKeyCode::Space, false);
        lInput.Update(1.0 / 60.0);
        lFixture.Input.EndFrame();

        CHECK(lOwnerA.Started == 0);
        CHECK(lOwnerA.Triggered == 0);
        CHECK(lOwnerB.Started == 1);
    }

    TEST_CASE("Unbind by handle removes exactly one, and a lambda binding works")
    {
        ContextFixture        lFixture;
        GameInstanceContext   lContext = lFixture.Make();
        InputMappingSubsystem lInput(lContext);

        lInput.RegisterAction(MakeAction("Jump", EInputValueType::Bool));

        InputMappingContext lMapping;
        lMapping.Name = Name("Gameplay");
        lMapping.Bindings.emplace_back(Bind("Jump", EKeyCode::Space));
        lInput.AddContext(lMapping);

        int lHits = 0;
        const DelegateHandle lHandle =
            lInput.Bind(Name("Jump"), EInputTrigger::Triggered,
                        [&lHits](const InputActionValue&) { ++lHits; });
        lInput.Bind(Name("Jump"), EInputTrigger::Triggered,
                    [&lHits](const InputActionValue&) { ++lHits; });
        CHECK(lInput.GetBindingCount() == 2);

        lFixture.Input.OnKeyPressed(EKeyCode::Space, false);
        lInput.Update(1.0 / 60.0);
        lFixture.Input.EndFrame();
        CHECK(lHits == 2);

        CHECK(lInput.Unbind(Name("Jump"), EInputTrigger::Triggered, lHandle));
        CHECK(lInput.GetBindingCount() == 1);

        lInput.Update(1.0 / 60.0);
        lFixture.Input.EndFrame();
        CHECK(lHits == 3);
    }

    TEST_CASE("GetValue reads the same table the callbacks fire from")
    {
        ContextFixture        lFixture;
        GameInstanceContext   lContext = lFixture.Make();
        InputMappingSubsystem lInput(lContext);

        lInput.RegisterAction(MakeAction("Move", EInputValueType::Axis1D));

        InputKeyBinding lLeft = Bind("Move", EKeyCode::A);
        lLeft.Modifiers.emplace_back(Negate());

        InputMappingContext lMapping;
        lMapping.Name = Name("Gameplay");
        lMapping.Bindings.emplace_back(Bind("Move", EKeyCode::D));
        lMapping.Bindings.emplace_back(lLeft);
        lInput.AddContext(lMapping);

        CHECK(lInput.GetActionCount() == 1);
        CHECK(lInput.GetContextCount() == 1);

        lFixture.Input.OnKeyPressed(EKeyCode::A, false);
        lInput.Update(1.0 / 60.0);
        lFixture.Input.EndFrame();

        CHECK(lInput.GetValue(Name("Move")).AsAxis1D() == doctest::Approx(-1.f));

        // An action nobody registered answers zero rather than throwing — it is a query.
        CHECK(lInput.GetValue(Name("Nope")).AsBool() == false);
        CHECK(lInput.FindState(Name("Nope")) == nullptr);
    }
}
