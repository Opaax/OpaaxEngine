#pragma once
#include "OpaaxInputEvents.hpp"
#include "OpaaxInputTypes.hpp"
#include "Core/OpaaxTypes.h"
#include "Core/Systems/EngineSubsystem.h"

namespace Opaax
{
    // =============================================================================
    // State array capacity
    //
    // EOpaaxKeyCode values:
    //   Keyboard    : 0   – 347
    //   Mouse       : 500 – 507
    //   Gamepad     : 1000 – 1013
    //
    // We use 1024 slots — covers keyboard and mouse with room to spare.
    // Gamepad values (1000+) fit within 1024 too.
    // EOpaaxKeyCode::Unknown (0xFFFFFFFF) is range-checked and silently ignored.
    // =============================================================================
    inline constexpr Uint16 INPUT_STATE_COUNT = 1024;
    inline constexpr Uint8  INPUT_STATE_RELEASED = 0;
    inline constexpr Uint8  INPUT_STATE_PRESSED  = 1;
 
    /**
     * @class InputSubsystem
     *
     * Tracks all input state across frames using double-buffering.
     * One unified state array covers keyboard, mouse buttons, and future gamepad —
     * indexed directly by EOpaaxKeyCode value.
     *
     * Query API (all O(1), zero allocation):
     *   IsKeyPressed(Key)           — held this frame
     *   IsKeyJustPressed(Key)       — went down this frame
     *   IsKeyJustReleased(Key)      — went up this frame
     *   GetMouseX() / GetMouseY()   — cursor position in window space
     *   GetMouseDeltaX/Y()          — movement since last frame
     *
     * NOTE: Mouse buttons query through the same IsKey* API using
     *   EOpaaxKeyCode::Mouse_Left etc. No separate mouse button functions needed.
     */
    class OPAAX_API InputSubsystem final : public EngineSubsystemBase
    {
        // =============================================================================
        // CTORs
        // =============================================================================
    public:
        InputSubsystem() = default;
        ~InputSubsystem() override = default;
        explicit InputSubsystem(CoreEngineApp* InEngineApp) : EngineSubsystemBase(InEngineApp) {}
        
        // =============================================================================
        // Function
        // =============================================================================
    private:
        using InputStateArray = TFixedArray<Uint8, INPUT_STATE_COUNT>;
 
        static FORCEINLINE bool GetState(const InputStateArray& InArray, EOpaaxKeyCode Key) noexcept
        {
            const Uint16 lIdx = static_cast<Uint16>(Key);
            return (lIdx < INPUT_STATE_COUNT) && InArray[lIdx];
        }
 
        FORCEINLINE void SetState(EOpaaxKeyCode Key, Uint8 bDown) noexcept
        {
            const Uint16 lIdx = static_cast<Uint16>(Key);
            if (lIdx < INPUT_STATE_COUNT) { m_Current[lIdx] = bDown; }
        }
 
        bool HandleKeyPressed(const KeyPressedEvent& Event);
        bool HandleKeyReleased(const KeyReleasedEvent& Event);
        bool HandleMouseButtonPressed(const MouseButtonPressedEvent& Event);
        bool HandleMouseButtonReleased(const MouseButtonReleasedEvent& Event);
        bool HandleMouseMoved(MouseMovedEvent& Event);

        //-------------------------------------------------------------------------------
        
    public:
        FORCEINLINE bool IsKeyPressed(EOpaaxKeyCode Key) const noexcept
        {
            return GetState(m_Current, Key);
        }
 
        FORCEINLINE bool IsKeyJustPressed(EOpaaxKeyCode Key) const noexcept
        {
            return GetState(m_Current, Key) && !GetState(m_Previous, Key);
        }
 
        FORCEINLINE bool IsKeyJustReleased(EOpaaxKeyCode Key) const noexcept
        {
            return !GetState(m_Current, Key) && GetState(m_Previous, Key);
        }

        //-------------------------------------------------------------------------------
        //Get - Set
 
        // Mouse position — window space, pixels, top-left origin
        FORCEINLINE float GetMouseX()      const noexcept { return m_MouseX; }
        FORCEINLINE float GetMouseY()      const noexcept { return m_MouseY; }
 
        // Movement since last frame — zero if no mouse event arrived this frame
        FORCEINLINE float GetMouseDeltaX() const noexcept { return m_MouseDeltaX; }
        FORCEINLINE float GetMouseDeltaY() const noexcept { return m_MouseDeltaY; }

        // =============================================================================
        // Override
        // =============================================================================
        //~Begin EngineSubsystemBase interface
    public:
        bool Startup()  override;
        void Shutdown() override;
        void Update(double DeltaTime) override;
        bool OnEvent(OpaaxEvent& Event) override;
        Uint32 GetEventCategoryFilter() const noexcept override;
        //~End EngineSubsystemBase interface
 
        // =============================================================================
        // Members
        // =============================================================================
    private:
        InputStateArray m_Current  {};   // this frame
        InputStateArray m_Previous {};   // last frame
 
        float m_MouseX      = 0.f;
        float m_MouseY      = 0.f;
        float m_MouseDeltaX = 0.f;
        float m_MouseDeltaY = 0.f;
        bool  m_bMouseMoved = false;
    };
}
