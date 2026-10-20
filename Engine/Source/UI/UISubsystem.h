#pragma once

#include "Core/EngineAPI.h"
#include "Core/Log/Logger.h"
#include "Engine/GameInstance/GameInstanceSubsystem.h"
#include "UI/UIInputMode.h"
#include "UI/UICanvas.h"

namespace Opaax
{
    struct LevelLoadFinished;
    struct LevelLoadRequested;
    class  InputMappingSubsystem;

    inline constexpr LogCategory LogUISubsystem{"UI"};

    // =============================================================================
    // UISubsystem — owns the game's UI canvas (HUD, menus). Lives for the whole game,
    //   across level changes. Submitted every frame.
    //
    //   The loading screen is a second canvas drawn on top, shown from a level request until
    //   the new level is ready (at least the project's loadingScreenMinSeconds).
    //   The canvas uses the project's UI reference height.
    //
    //   Gameplay reaches it through WorldContext::UI.
    // =============================================================================
    class UISubsystem final : public GameInstanceSubsystem
    {
        OPAAX_OBJECT(UISubsystem, GameInstanceSubsystem)

        // =========================================================================
        // CTORS - DTORS
        // =========================================================================
    public:
        UISubsystem();
        ~UISubsystem() override;

        // =========================================================================
        // Canvas
        // =========================================================================
    public:
        UICanvas&       GetCanvas()       noexcept { return m_Canvas; }
        const UICanvas& GetCanvas() const noexcept { return m_Canvas; }

        /**
         * Loads a .opaaxui and adds its tree under InParent (the root when null).
         * Warns once if the asset was designed at another height.
         * @return The tree's root (owned by the canvas), or null if it did not load
         */
        UIWidget* MountAsset(const OpaaxString& InAssetPath, UIWidget* InParent = nullptr);

        /** Loads a .opaaxui tree without adding it (for another canvas). */
        TUniquePtr<UIWidget> LoadTree(const OpaaxString& InAssetPath, float& OutAuthoredHeight) const;

        // =========================================================================
        // Loading screen
        // =========================================================================
    public:
        /** The loading screen's canvas. */
        UICanvas& GetLoadingCanvas() noexcept { return m_LoadingCanvas; }

        /** Shown from RequestOpenLevel until the level is ready. */
        bool IsCoverUp() const noexcept { return m_LoadingCanvas.Root().bVisible; }

        // =========================================================================
        // Input mode
        // =========================================================================
    public:
        void         SetInputMode(EUIInputMode InMode) noexcept;
        EUIInputMode GetInputMode() const noexcept { return m_InputMode; }

        // =========================================================================
        // Override
        // =========================================================================
    public:
        //~Begin Subsystem interface
        /** After the global audio bank, before the input mapping. */
        Int32 GetTickOrder() const override { return 100; }

        bool Startup() override;
        void Update(double InDeltaTime) override;
        void Shutdown() override;
        //~End Subsystem interface

        // =========================================================================
        // Internal
        // =========================================================================
    private:
        /** Loads the project's loading screen, or a black image if none. */
        void BuildLoadingCover();

        void OnLevelLoadRequested(const LevelLoadRequested&);
        void OnLevelLoadFinished(const LevelLoadFinished&);

        // =========================================================================
        // Members
        // =========================================================================
    private:
        InputMappingSubsystem* m_Mapping = nullptr;   // resolved in Startup; may be null
        UICanvas               m_Canvas;
        UICanvas               m_LoadingCanvas;
        EUIInputMode           m_InputMode = EUIInputMode::GameAndUI;

        /** Loading screen timer: time shown, minimum time, and whether loading is done. */
        double m_CoverElapsed    = 0.0;
        float  m_CoverMinSeconds = 0.f;
        bool   m_bLoadFinished   = false;
    };
}
