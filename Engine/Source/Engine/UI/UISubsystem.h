#pragma once

#include "Core/EngineAPI.h"
#include "Application/Services/ILogger.h"
#include "Engine/GameInstance/IGameInstanceSubsystem.h"
#include "UI/UICanvas.h"

namespace Opaax
{
    struct GameInstanceContext;

    inline constexpr LogCategory LogUISubsystem{"UI"};

    // =============================================================================
    // UISubsystem — the GameInstance's UI tenant: it OWNS the persistent canvas.
    //
    //   The game outlives every world it plays through (GI1), so what hangs here — the HUD, a
    //   pause menu, a loading cover — stays up across a level swap and is torn down whole at
    //   EndGame (GI6). The canvas is submitted every frame (the view idiom, F4): the renderer
    //   lays it out for each target that opted in and composites it over the world.
    //
    //   Gameplay reaches it through WorldContext::UI, the way it reaches input mapping.
    // =============================================================================
    class OPAAX_API UISubsystem final : public GameInstanceSubsystemBase
    {
        // =========================================================================
        // Base Implementation
        // =========================================================================
    public:
        OPAAX_SUBSYSTEM_TYPE(UISubsystem)

        // =========================================================================
        // CTORS - DTORS
        // =========================================================================
    public:
        explicit UISubsystem(GameInstanceContext& InContext) noexcept;
        ~UISubsystem() override = default;

        // =========================================================================
        // Canvas
        // =========================================================================
    public:
        UICanvas&       GetCanvas()       noexcept { return m_Canvas; }
        const UICanvas& GetCanvas() const noexcept { return m_Canvas; }

        // =========================================================================
        // ISubsystem
        // =========================================================================
    public:
        bool Startup() override;
        void Update(double InDeltaTime) override;
        void Shutdown() override;

        // =========================================================================
        // Members
        // =========================================================================
    private:
        GameInstanceContext* m_Context = nullptr;   // borrowed; the GameInstance owns it
        UICanvas             m_Canvas;
    };
}
