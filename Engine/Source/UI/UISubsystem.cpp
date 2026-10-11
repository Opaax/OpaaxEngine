#include "UI/UISubsystem.h"

#include "Application/OpaaxApplication.h"
#include "Application/Services/IEngine.h"
#include "Application/Services/IPaths.h"
#include "Application/Services/IProjectManager.h"
#include "Engine/EngineEvents.h"
#include "Engine/GameInstance/GameInstance.h"
#include "Engine/GameInstance/GameInstanceContext.h"
#include "Engine/GameInstance/GameInstanceManager.h"
#include "Input/Mapping/InputMappingSubsystem.h"
#include "Engine/Registries/EngineRegistries.h"
#include "Engine/Subsystems/EngineEventBus.h"
#include "Resources/ResourceManager.h"
#include "UI/UICanvasResource.h"
#include "UI/UIInputRouter.h"
#include "UI/UICanvasFile.h"
#include "UI/Widgets/UIImage.h"

namespace Opaax
{
    UISubsystem::UISubsystem(GameInstanceContext& InContext) noexcept
        : m_Context(&InContext)
    {
    }

    bool UISubsystem::Startup()
    {
        // Every subsystem exists before any Startup, so the sibling is found whatever the order.
        // May be null (a game without input mapping).
        if (GameInstance* lGame = OpaaxApplication::GetAppService<IEngine>().GetGameInstances().GetGameInstance())
        {
            m_Mapping = lGame->GetSubsystems().GetSubsystem<InputMappingSubsystem>();
        }

        // Every UI asset uses the project's reference height.
        m_Canvas.SetReferenceHeight(OpaaxApplication::GetAppService<IProjectManager>().UIReferenceHeight());

        BuildLoadingCover();

        m_Context->Events.GetEventBus().Subscribe<LevelLoadRequested>(this, &UISubsystem::OnLevelLoadRequested);
        m_Context->Events.GetEventBus().Subscribe<LevelLoadFinished>(this, &UISubsystem::OnLevelLoadFinished);

        return true;
    }

    TUniquePtr<UIWidget> UISubsystem::LoadTree(const OpaaxString& InAssetPath, float& OutAuthoredHeight) const
    {
        const OpaaxString lAbsPath = m_Context->Paths.AssetToAbsolute(InAssetPath);

        const ResourceRef<UICanvasResource> lRef      = m_Context->Resources.Load<UICanvasResource>(lAbsPath.CStr());
        const UICanvasResource* const       lResource = lRef.IsValid() ? lRef.Get() : nullptr;

        if (lResource == nullptr)
        {
            OPAAX_LOG(LogUISubsystem, Error, "UI asset '{}' did not load", InAssetPath.CStr());
            return nullptr;
        }

        TUniquePtr<UIWidget> lTree =
            lResource->BuildTree(OpaaxApplication::GetAppService<IEngine>().GetRegistries().UIWidgets(), OutAuthoredHeight);

        if (!lTree)
        {
            OPAAX_LOG(LogUISubsystem, Error, "UI asset '{}' did not parse", InAssetPath.CStr());
        }

        return lTree;
    }

    UIWidget* UISubsystem::MountAsset(const OpaaxString& InAssetPath, UIWidget* InParent)
    {
        float lAuthoredHeight = 0.f;
        TUniquePtr<UIWidget> lTree = LoadTree(InAssetPath, lAuthoredHeight);
        if (!lTree)
        {
            return nullptr;   // LoadTree logged why
        }

        // Warn when the asset was designed at another height than the project's.
        if (lAuthoredHeight != m_Canvas.GetReferenceHeight())
        {
            OPAAX_LOG(LogUISubsystem, Warn, "'{}' was authored at a reference height of {} but the canvas is {} — it will not look like the panel (set uiReferenceHeight in the project, or re-save the asset at {})",
                      InAssetPath.CStr(), lAuthoredHeight, m_Canvas.GetReferenceHeight(), m_Canvas.GetReferenceHeight());
        }

        UIWidget& lParent  = InParent != nullptr ? *InParent : m_Canvas.Root();
        UIWidget* lMounted = lParent.AddChild(Move(lTree));

        OPAAX_LOG(LogUISubsystem, Trace, "Mounted '{}' — {} widget(s) under '{}'",
                  InAssetPath.CStr(), UICanvasFile::CountWidgets(*lMounted), lParent.Name.CStr());
        return lMounted;
    }

    void UISubsystem::BuildLoadingCover()
    {
        UIWidget& lRoot = m_LoadingCanvas.Root();
        lRoot.bVisible  = false;   // hidden until a level is requested

        const IProjectManager& lProject = OpaaxApplication::GetAppService<IProjectManager>();
        const OpaaxString      lAsset   = lProject.LoadingScreen();
        m_CoverMinSeconds = lProject.LoadingScreenMinSeconds();

        if (!lAsset.IsEmpty())
        {
            // Alone on its canvas, so the asset's own height is used.
            float lAuthoredHeight = 0.f;
            if (TUniquePtr<UIWidget> lTree = LoadTree(lAsset, lAuthoredHeight))
            {
                m_LoadingCanvas.SetReferenceHeight(lAuthoredHeight);
                lRoot.AddChild(Move(lTree));

                OPAAX_LOG(LogUISubsystem, Trace, "Loading cover: '{}' — {} widget(s), up at least {} s",
                          lAsset.CStr(), UICanvasFile::CountWidgets(lRoot), m_CoverMinSeconds);
                return;
            }

            OPAAX_LOG(LogUISubsystem, Warn, "Loading screen '{}' did not load — a black cover stands in.", lAsset.CStr());
        }

        auto lBlack = MakeUnique<UIImage>();
        lBlack->Name = "Cover";
        lBlack->SetColor({ 0.f, 0.f, 0.f, 1.f });

        UIRect lStretched;
        lStretched.AnchorMin = { 0.f, 0.f };
        lStretched.AnchorMax = { 1.f, 1.f };
        lStretched.SizeDelta = { 0.f, 0.f };
        lBlack->SetRect(lStretched);

        lRoot.AddChild(Move(lBlack));

        OPAAX_LOG(LogUISubsystem, Trace, "Loading cover: black (the project names no loadingScreen)");
    }

    void UISubsystem::OnLevelLoadRequested(const LevelLoadRequested&)
    {
        // Visibility is read at draw time, so this frame is already covered.
        // A new request while visible restarts the timer.
        m_LoadingCanvas.Root().bVisible = true;
        m_CoverElapsed  = 0.0;
        m_bLoadFinished = false;
    }

    void UISubsystem::OnLevelLoadFinished(const LevelLoadFinished&)
    {
        // Only the loading screen is ours to hide, once the minimum time is reached.
        m_bLoadFinished = true;
    }

    void UISubsystem::SetInputMode(const EUIInputMode InMode) noexcept
    {
        m_InputMode = InMode;
    }

    void UISubsystem::Update(const double InDeltaTime)
    {
        if (m_LoadingCanvas.Root().bVisible)
        {
            m_CoverElapsed += InDeltaTime;

            if (m_bLoadFinished && m_CoverElapsed >= m_CoverMinSeconds)
            {
                m_LoadingCanvas.Root().bVisible = false;
                OPAAX_LOG(LogUISubsystem, Trace, "Loading cover down after {:.2f} s (floor {} s)", m_CoverElapsed, m_CoverMinSeconds);
            }
        }

        // Route the input to the canvas and report what the UI used, before input mapping runs.
        InputKeyMask lConsumed{};
        UIInputRouter::Route(m_Context->Input, m_Canvas, m_InputMode, lConsumed);

        if (m_Mapping != nullptr)
        {
            m_Mapping->ConsumeThisFrame(lConsumed);
        }

        // Submitted every frame. The loading screen goes after, so it draws on top.
        IEngine& lEngine = OpaaxApplication::GetAppService<IEngine>();
        lEngine.SubmitUICanvas(m_Canvas);
        lEngine.SubmitUICanvas(m_LoadingCanvas);
    }

    void UISubsystem::Shutdown()
    {
        m_Context->Events.GetEventBus().UnsubscribeAll(this);
    }
}
