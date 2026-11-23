#pragma once

#include "Core/EngineAPI.h"
#include "Core/Log/Logger.h"
#include "Engine/GameInstance/GameInstanceSubsystem.h"
#include "UI/UIInputMode.h"
#include "UI/UIAudioPlayer.h"
#include "UI/UICanvas.h"
#include "UI/UILayerSettings.h"

namespace Opaax
{
    struct LevelLoadFinished;
    struct LevelLoadRequested;
    class  Config_Engine;
    class  InputMappingSubsystem;

    inline constexpr LogCategory LogUISubsystem{"UI"};

    // =============================================================================
    // UISubsystem — owns the game's UI canvas (HUD, menus). Lives for the whole game,
    //   across level changes. Submitted every frame.
    //
    //   Its trees draw by layer, the engine config's UI.Layers (Hud, Menu, Debug, Loading by default),
    //   a higher layer on top whatever the order they were mounted in. An edit of the list applies at once.
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
         * Loads a .opaaxui and adds its tree under InParent (the root when null), in InLayer: under the
         * root, over the trees of a lower layer and those of its own mounted before, under a higher layer.
         * A layer the engine config does not list draws in its nearest listed parent ("Menu.Extra" in
         * Menu), with a warning once. Warns once if the asset was designed at another height.
         * @return The tree's root (owned by the canvas), or null if it did not load
         */
        UIWidget* MountAsset(const OpaaxString& InAssetPath, UIWidget* InParent = nullptr,
                             OpaaxTag InLayer = UILayerHud());

        /** Loads a .opaaxui tree without adding it (for another canvas). */
        TStrongObjectPtr<UIWidget> LoadTree(const OpaaxString& InAssetPath, float& OutAuthoredHeight) const;

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

        /** The engine config's UI.Layers to the canvas, when the list changed; what is wrong with it is logged. */
        void ApplyLayers();

        // =========================================================================
        // Members
        // =========================================================================
    private:
        InputMappingSubsystem* m_Mapping = nullptr;   // resolved in Startup; may be null
        Config_Engine*         m_EngineConfig = nullptr;   // resolved in Startup
        /** The UI.Layers the canvas has, and the unlisted layers already warned about. */
        TDynArray<UILayerSettings> m_AppliedLayers;
        TUnorderedSet<OpaaxTag>    m_WarnedLayers;
        UICanvas               m_Canvas;
        UICanvas               m_LoadingCanvas;
        EUIInputMode           m_InputMode = EUIInputMode::GameAndUI;
        /** The canvases' sounds, on the UI bus: a paused world does not silence them. */
        TUniquePtr<UIAudioPlayer> m_Audio;

        /** Loading screen timer: time shown, minimum time, and whether loading is done. */
        double m_CoverElapsed    = 0.0;
        float  m_CoverMinSeconds = 0.f;
        bool   m_bLoadFinished   = false;
    };
}
