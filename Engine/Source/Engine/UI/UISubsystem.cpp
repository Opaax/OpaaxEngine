#include "Engine/UI/UISubsystem.h"

#include "Application/OpaaxApplication.h"
#include "Application/Services/IEngine.h"
#include "Engine/GameInstance/GameInstance.h"
#include "Engine/GameInstance/GameInstanceContext.h"
#include "Engine/GameInstance/GameInstanceManager.h"
#include "Engine/Input/InputMappingSubsystem.h"
#include "Engine/UI/UIInputRouter.h"

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

        OPAAX_LOG(LogUISubsystem, Info, "UI started — canvas reference height {}, mode {}, mapping {}",
                  m_Canvas.GetReferenceHeight(), ToString(m_InputMode), m_Mapping != nullptr ? "linked" : "absent");
        return true;
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

        // Every frame, like a view: the renderer draws what was submitted and forgets it.
        OpaaxApplication::GetAppService<IEngine>().SubmitUICanvas(m_Canvas);
    }

    void UISubsystem::Shutdown()
    {
        OPAAX_LOG(LogUISubsystem, Info, "UI shutdown — {} root child(ren) dropped with the canvas",
                  m_Canvas.Root().GetChildren().size());
    }
}
