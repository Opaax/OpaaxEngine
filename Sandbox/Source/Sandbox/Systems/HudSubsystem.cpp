#include "Systems/HudSubsystem.h"

#include <algorithm>

#include <glm/geometric.hpp>   // glm::length

#include "Application/OpaaxApplication.h"
#include "Application/Services/ILogger.h"
#include "Application/Services/IEngine.h"
#include "Application/Services/IPaths.h"
#include "Engine/Input/InputMappingSubsystem.h"
#include "Engine/Input/InputTypes.h"
#include "Engine/Registries/EngineRegistries.h"
#include "Engine/Subsystems/Resources/ResourceManager.h"
#include "Engine/Subsystems/Resources/Types/UI/UICanvasResource.h"
#include "Engine/UI/UISubsystem.h"
#include "UI/UICanvas.h"
#include "UI/Widgets/UIImage.h"
#include "UI/Widgets/UIText.h"
#include "World/Components/MoverComponent.h"
#include "World/Systems/WorldContext.h"
#include "World/World.h"

using namespace Opaax;

namespace Sandbox
{
    namespace
    {
        constexpr LogCategory LogHud{"Hud"};

        const OpaaxStringID kJumpAction = OPAAX_ID("Jump");

        /** The authored tree, and the two widgets this drives inside it. */
        constexpr const char* kHudAsset  = "UI/Hud.opaaxui";
        constexpr const char* kJumpsName = "Jumps";
        constexpr const char* kSpeedName = "SpeedFill";

        /** MoveModeData's default MaxSpeed — the bar is full at it. A constant until a HUD reads tunings. */
        constexpr float kFullSpeed = 400.f;
    }

    bool HudSubsystem::ShouldCreate(const World& InWorld)
    {
        return InWorld.GetMode() == EWorldMode::Play;
    }

    bool HudSubsystem::Startup()
    {
        if (m_Context->UI == nullptr)
        {
            OPAAX_LOG(LogHud, Error, "HUD started with NO UI subsystem — this world has no game instance, nothing is drawn.");
            return true;
        }

        IEngine&          lEngine  = OpaaxApplication::GetAppService<IEngine>();
        const OpaaxString lAbsPath = m_Context->Paths.AssetToAbsolute(OpaaxString(kHudAsset));

        ResourceRef<UICanvasResource> lRef = m_Context->Resources.Load<UICanvasResource>(lAbsPath.CStr());
        const UICanvasResource* const lResource = lRef.IsValid() ? lRef.Get() : nullptr;

        if (lResource == nullptr)
        {
            OPAAX_LOG(LogHud, Error, "HUD asset '{}' did not load — no HUD is drawn.", kHudAsset);
            return true;
        }

        float lReferenceHeight = 0.f;
        TUniquePtr<UIWidget> lTree = lResource->BuildTree(lEngine.GetRegistries().UIWidgets(), lReferenceHeight);

        if (!lTree)
        {
            OPAAX_LOG(LogHud, Error, "HUD asset '{}' did not parse — no HUD is drawn.", kHudAsset);
            return true;
        }

        UICanvas& lCanvas = m_Context->UI->GetCanvas();
        m_Root = lCanvas.Root().AddChild(Move(lTree));

        // Bound BY NAME out of the authored tree (UI13). A rename is a quiet HUD, so it says so.
        m_Jumps = static_cast<UIText*>(m_Root->FindByName(OpaaxString(kJumpsName)));
        m_Speed = static_cast<UIImage*>(m_Root->FindByName(OpaaxString(kSpeedName)));

        if (m_Jumps == nullptr || m_Speed == nullptr)
        {
            OPAAX_LOG(LogHud, Error, "HUD loaded but '{}' or '{}' is not in it — that piece stays static.",
                      kJumpsName, kSpeedName);
        }

        if (m_Context->Actions != nullptr)
        {
            m_Context->Actions->Bind(kJumpAction, EInputTrigger::Started, this, &HudSubsystem::OnJump);
        }

        OPAAX_LOG(LogHud, Info, "HUD loaded '{}' — {} widget(s), reference height {}, Jumps {}, SpeedFill {}",
                  kHudAsset, UICanvasFile::CountWidgets(*m_Root), lReferenceHeight,
                  m_Jumps != nullptr ? "bound" : "MISSING", m_Speed != nullptr ? "bound" : "MISSING");
        return true;
    }

    void HudSubsystem::Update(double)
    {
        if (m_Speed == nullptr)
        {
            return;
        }

        float lFastest = 0.f;
        m_Context->OwningWorld.Each<MoverComponent>([&lFastest](EntityID, const MoverComponent& InMover)
        {
            lFastest = std::max(lFastest, glm::length(InMover.Velocity));
        });

        const float lAmount = std::clamp(lFastest / kFullSpeed, 0.f, 1.f);
        if (lAmount != m_Speed->FillAmount)
        {
            m_Speed->SetFillAmount(lAmount);
        }
    }

    void HudSubsystem::Shutdown()
    {
        if (m_Context->Actions != nullptr)
        {
            m_Context->Actions->UnbindAll(this);
        }

        // The canvas outlives this world (GI1); the HUD does not.
        if (m_Root != nullptr && m_Context->UI != nullptr)
        {
            m_Context->UI->GetCanvas().Root().RemoveChild(*m_Root);
        }

        m_Root  = nullptr;
        m_Jumps = nullptr;
        m_Speed = nullptr;

        OPAAX_LOG(LogHud, Info, "HUD shutdown — {} jump(s) counted", m_JumpCount);
    }

    void HudSubsystem::OnJump(const InputActionValue& /*InValue*/)
    {
        // Bindings outlive worlds (IM8): the outgoing world's HUD hears this too.
        if (!m_Context->OwningWorld.IsActive() || m_Jumps == nullptr)
        {
            return;
        }

        ++m_JumpCount;
        m_Jumps->SetText(OpaaxString("Jumps: ") + OpaaxString::FromUInt(m_JumpCount));
    }
}
