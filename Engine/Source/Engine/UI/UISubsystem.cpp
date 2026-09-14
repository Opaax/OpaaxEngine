#include "Engine/UI/UISubsystem.h"

#include "Application/OpaaxApplication.h"
#include "Application/Services/IEngine.h"
#include "Engine/GameInstance/GameInstanceContext.h"

namespace Opaax
{
    UISubsystem::UISubsystem(GameInstanceContext& InContext) noexcept
        : m_Context(&InContext)
    {
    }

    bool UISubsystem::Startup()
    {
        OPAAX_LOG(LogUISubsystem, Info, "UI started — canvas reference height {}", m_Canvas.GetReferenceHeight());
        return true;
    }

    void UISubsystem::Update(double /*InDeltaTime*/)
    {
        // Every frame, like a view: the renderer draws what was submitted and forgets it.
        OpaaxApplication::GetAppService<IEngine>().SubmitUICanvas(m_Canvas);
    }

    void UISubsystem::Shutdown()
    {
        OPAAX_LOG(LogUISubsystem, Info, "UI shutdown — {} root child(ren) dropped with the canvas",
                  m_Canvas.Root().GetChildren().size());
    }
}
