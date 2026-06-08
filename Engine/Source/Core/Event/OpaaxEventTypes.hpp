#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"

namespace Opaax
{
    // =============================================================================
    // EEventType
    //
    // Every concrete event maps to exactly one value here.
    // Grouped by domain — keep groups together when extending.
    //
    // HOW TO ADD A NEW ENGINE EVENT TYPE:
    //   1. Add an entry in the correct group below (or add a new group).
    //   2. Write the event class in the appropriate header (ApplicationEvent.h,
    //      KeyEvent.h, MouseEvent.h, or a new file for a new domain).
    //   3. No registration needed — the type enum is all the system requires.
    //
    // HOW TO ADD A GAME-LAYER EVENT TYPE:
    //   1. Add an entry in the "Gameplay" block below.
    //   2. Write your event class anywhere in the game project — it does NOT
    //      need to live in the engine. Include OpaaxEvent.h and use the macros.
    //   Example:
    //       class PlayerDiedEvent final : public Opaax::OpaaxEvent {
    //           OPAAX_EVENT_CLASS_TYPE(Opaax::EEventType::PlayerDied)
    //           OPAAX_EVENT_CLASS_CATEGORY(Opaax::EEventCategory_Gameplay)
    //       };
    // =============================================================================
    enum class EEventType : Uint16
    {
        None = 0,
 
        // --- Window / Application ---
        WindowClose,
        WindowResize,
        WindowFocus,
        WindowLostFocus,
        WindowMoved,
 
        // --- Keyboard ---
        KeyPressed,
        KeyReleased,
        KeyTyped,
 
        // --- Mouse ---
        MouseButtonPressed,
        MouseButtonReleased,
        MouseMoved,
        MouseScrolled,

        // --- Physics (engine domain; dispatched from PhysicsSubsystem after the step) ---
        // NOTE: Overlap (Mode==Overlap collider) carries a synthesized per-step Tick; solid
        //   collisions are Enter/Exit only (Box2D's native begin/end touch).
        OverlapStart,
        OverlapTick,
        OverlapStop,
        CollisionEnter,
        CollisionExit,

        // --- Gameplay (game project adds entries here) ---
        // NOTE: Keep engine types above this line. Gameplay types below.
        //   This ordering is a convention, not enforced by the compiler.
        //   The subsystem filter (GetEventCategoryFilter) ensures engine subsystems
        //   never receive gameplay events even if entries are interleaved.
        GameplayEvent = 1000,  // sentinel — reserve space, avoid collisions

        // --- Editor (editor-only; gated by OPAAX_WITH_EDITOR at consumer sites) ---
        // NOTE: Editor events flow through EditorEventBus, NOT through the engine
        //   subsystem dispatch path. The sentinel exists here only to keep the
        //   global EEventType space collision-free.
        EditorEvent = 2000,  // sentinel
        EntitySelected,
        SceneSaved,
        NewScene,
        AssetImported,
        AssetSelected,
    };
 
    // =============================================================================
    // EEventCategory — bitmask flags
    //
    // A single event can belong to multiple categories.
    // Used by IEngineSubsystem::GetEventCategoryFilter() to opt-in to event types
    // without receiving every event broadcast.
    //
    // HOW TO ADD A CATEGORY:
    //   Add a BIT(N) entry. Max 32 categories with Uint32.
    //   If you need more, change the underlying type to Uint64 here and in
    //   OpaaxEvent::GetCategoryFlags() / IsInCategory().
    // =============================================================================
    enum EEventCategory : Uint16
    {
        EEventCategory_None        = 0,
        EEventCategory_Application = BIT(0),   // window events
        EEventCategory_Input       = BIT(1),   // any input
        EEventCategory_Keyboard    = BIT(2),   // keyboard specifically
        EEventCategory_Mouse       = BIT(3),   // mouse move / scroll
        EEventCategory_MouseButton = BIT(4),   // mouse button press/release
        EEventCategory_Gameplay    = BIT(5),   // game-layer events (not used by engine subsystems)
        EEventCategory_Editor      = BIT(6),   // editor-only events dispatched via EditorEventBus
        EEventCategory_Physics     = BIT(7),   // overlap / collision events from PhysicsSubsystem
        // BIT(8..31) reserved for future engine domains or game extension
    };
}