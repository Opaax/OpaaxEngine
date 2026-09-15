#pragma once

#include "Core/EngineAPI.h"
#include "Application/Services/ILogger.h"
#include "Engine/GameInstance/IGameInstanceSubsystem.h"
#include "Engine/UI/UIInputMode.h"
#include "UI/UICanvas.h"

namespace Opaax
{
    struct GameInstanceContext;
    struct LevelLoadFinished;
    struct LevelLoadRequested;
    class  InputMappingSubsystem;

    inline constexpr LogCategory LogUISubsystem{"UI"};

    // =============================================================================
    // UISubsystem — the GameInstance's UI tenant: it OWNS the persistent canvas.
    //
    //   The game outlives every world it plays through (GI1), so what hangs here — the HUD, a
    //   pause menu — stays up across a level swap and is torn down whole at EndGame (GI6). The
    //   canvas is submitted every frame (the view idiom, F4): the renderer lays it out for each
    //   target that opted in and composites it over the world.
    //
    //   THE LOADING COVER IS A SECOND CANVAS (UI21), submitted after the first every frame with
    //   its root's visibility as the flag. Visibility is read at DRAW time (UI3), so a level
    //   requested mid-frame — a button in this tenant's own tick, a trigger in the world's — is
    //   covered on THAT frame's render, whichever of the two it came from. Its tree is the
    //   project's `loadingScreen` asset; black when the project names none.
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
        // The loading cover (UI21) — drawn over everything while a level is on its way
        // =========================================================================
    public:
        /** The cover's own canvas, for a game that binds a progress bar into it later. */
        UICanvas& GetLoadingCanvas() noexcept { return m_LoadingCanvas; }

        /** Up from a RequestOpenLevel until the swap is done. */
        bool IsCoverUp() const noexcept { return m_LoadingCanvas.Root().bVisible; }

        // =========================================================================
        // Input mode (UI10) — Unreal's three, on the session
        // =========================================================================
    public:
        void         SetInputMode(EUIInputMode InMode) noexcept;
        EUIInputMode GetInputMode() const noexcept { return m_InputMode; }

        // =========================================================================
        // ISubsystem
        // =========================================================================
    public:
        bool Startup() override;
        void Update(double InDeltaTime) override;
        void Shutdown() override;

        // =========================================================================
        // Internal
        // =========================================================================
    private:
        /** The project's loading screen into the cover canvas, or a black image when it names none. */
        void BuildLoadingCover();

        void OnLevelLoadRequested(const LevelLoadRequested&);
        void OnLevelLoadFinished(const LevelLoadFinished&);

        // =========================================================================
        // Members
        // =========================================================================
    private:
        GameInstanceContext*   m_Context = nullptr;   // borrowed; the GameInstance owns it
        InputMappingSubsystem* m_Mapping = nullptr;   // sibling tenant, resolved in Startup; may be null
        UICanvas               m_Canvas;
        UICanvas               m_LoadingCanvas;
        EUIInputMode           m_InputMode = EUIInputMode::GameAndUI;
    };
}
