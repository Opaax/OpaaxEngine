#include "Engine/UI/UISubsystem.h"

#include "Application/OpaaxApplication.h"
#include "Application/Services/IEngine.h"
#include "Application/Services/IPaths.h"
#include "Application/Services/IProjectManager.h"
#include "Engine/EngineEvents.h"
#include "Engine/GameInstance/GameInstance.h"
#include "Engine/GameInstance/GameInstanceContext.h"
#include "Engine/GameInstance/GameInstanceManager.h"
#include "Engine/Input/InputMappingSubsystem.h"
#include "Engine/Registries/EngineRegistries.h"
#include "Engine/Subsystems/EventBus/EngineEventBus.h"
#include "Engine/Subsystems/Resources/ResourceManager.h"
#include "Engine/Subsystems/Resources/Types/UI/UICanvasResource.h"
#include "Engine/UI/UIInputRouter.h"
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
        // Resolve the sibling tenant: the create pass builds every subsystem before any Startup, so
        // it exists whatever the registration order (F3, one tier down). Null-tolerant — a game
        // without input mapping is odd but not a crash here.
        if (GameInstance* lGame = OpaaxApplication::GetAppService<IEngine>().GetGameInstances().GetGameInstance())
        {
            m_Mapping = lGame->GetSubsystems().GetSubsystem<InputMappingSubsystem>();
        }

        BuildLoadingCover();

        m_Context->Events.GetEventBus().Subscribe<LevelLoadRequested>(this, &UISubsystem::OnLevelLoadRequested);
        m_Context->Events.GetEventBus().Subscribe<LevelLoadFinished>(this, &UISubsystem::OnLevelLoadFinished);

        OPAAX_LOG(LogUISubsystem, Info, "UI started — canvas reference height {}, mode {}, mapping {}",
                  m_Canvas.GetReferenceHeight(), ToString(m_InputMode), m_Mapping != nullptr ? "linked" : "absent");
        return true;
    }

    void UISubsystem::BuildLoadingCover()
    {
        UIWidget& lRoot = m_LoadingCanvas.Root();
        lRoot.bVisible  = false;   // down until a level is asked for

        const OpaaxString lAsset = OpaaxApplication::GetAppService<IProjectManager>().LoadingScreen();

        if (!lAsset.IsEmpty())
        {
            const OpaaxString lAbsPath = m_Context->Paths.AssetToAbsolute(lAsset);

            const ResourceRef<UICanvasResource> lRef = m_Context->Resources.Load<UICanvasResource>(lAbsPath.CStr());
            const UICanvasResource* const       lResource = lRef.IsValid() ? lRef.Get() : nullptr;

            float lReferenceHeight = 0.f;
            TUniquePtr<UIWidget> lTree = lResource != nullptr
                ? lResource->BuildTree(OpaaxApplication::GetAppService<IEngine>().GetRegistries().UIWidgets(), lReferenceHeight)
                : nullptr;

            if (lTree)
            {
                m_LoadingCanvas.SetReferenceHeight(lReferenceHeight);
                lRoot.AddChild(Move(lTree));

                OPAAX_LOG(LogUISubsystem, Info, "Loading cover: '{}' — {} widget(s)",
                          lAsset.CStr(), UICanvasFile::CountWidgets(lRoot));
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

        OPAAX_LOG(LogUISubsystem, Info, "Loading cover: black (the project names no loadingScreen)");
    }

    void UISubsystem::OnLevelLoadRequested(const LevelLoadRequested&)
    {
        // Visibility is read when the canvas is DRAWN, so this covers the frame the request came
        // in on, wherever in the frame that was.
        m_LoadingCanvas.Root().bVisible = true;
    }

    void UISubsystem::OnLevelLoadFinished(const LevelLoadFinished&)
    {
        // The old world's widgets left through RemoveChild, which already forgot any pointer under
        // them (UI9) — only the cover is this tenant's to bring down.
        m_LoadingCanvas.Root().bVisible = false;
    }

    void UISubsystem::SetInputMode(const EUIInputMode InMode) noexcept
    {
        m_InputMode = InMode;
    }

    void UISubsystem::Update(double /*InDeltaTime*/)
    {
        // Route the raw feed through the canvas, and tell the mapping what the UI swallowed BEFORE
        // it evaluates — this tenant is registered ahead of input mapping so its Update runs first.
        InputKeyMask lConsumed{};
        UIInputRouter::Route(m_Context->Input, m_Canvas, m_InputMode, lConsumed);

        if (m_Mapping != nullptr)
        {
            m_Mapping->ConsumeThisFrame(lConsumed);
        }

        // Every frame, like a view: the renderer draws what was submitted and forgets it. The cover
        // goes AFTER, so it draws over — and a hidden root costs no pass (UI21).
        IEngine& lEngine = OpaaxApplication::GetAppService<IEngine>();
        lEngine.SubmitUICanvas(m_Canvas);
        lEngine.SubmitUICanvas(m_LoadingCanvas);
    }

    void UISubsystem::Shutdown()
    {
        m_Context->Events.GetEventBus().UnsubscribeAll(this);

        OPAAX_LOG(LogUISubsystem, Info, "UI shutdown — {} root child(ren) dropped with the canvas",
                  m_Canvas.Root().GetChildren().size());
    }
}
