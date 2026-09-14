#include "Systems/HudSubsystem.h"

#include <algorithm>

#include <glm/geometric.hpp>   // glm::length

#include "Application/Services/ILogger.h"
#include "Engine/Input/InputMappingSubsystem.h"
#include "Engine/Input/InputTypes.h"
#include "Engine/UI/UISubsystem.h"
#include "UI/UICanvas.h"
#include "UI/Widgets/UIImage.h"
#include "UI/Widgets/UIPanel.h"
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

        constexpr const char* kFace = "/Engine/Fonts/Roboto/roboto-latin-700-normal.ttf";

        /** MoveModeData's default MaxSpeed — the bar is full at it. A constant until a HUD reads tunings. */
        constexpr float kFullSpeed = 400.f;

        UIRect Corner(const Vector2F& InAnchor, const Vector2F& InOffset, const Vector2F& InSize)
        {
            UIRect lRect;
            lRect.AnchorMin = lRect.AnchorMax = lRect.Pivot = InAnchor;
            lRect.AnchoredPosition = InOffset;
            lRect.SizeDelta        = InSize;
            return lRect;
        }

        UIRect Stretched()
        {
            UIRect lRect;
            lRect.AnchorMin = { 0.f, 0.f };
            lRect.AnchorMax = { 1.f, 1.f };
            lRect.SizeDelta = { 0.f, 0.f };
            return lRect;
        }
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

        UICanvas& lCanvas = m_Context->UI->GetCanvas();

        // One node of mine under the canvas root, so Shutdown takes exactly one thing back.
        m_Panel = lCanvas.Root().AddChild(MakeUnique<UIPanel>());
        m_Panel->Name = "Hud";
        m_Panel->SetRect(Stretched());

        auto lJumps = MakeUnique<UIText>();
        lJumps->Name = "Jumps";
        lJumps->SetRect(Corner({ 0.f, 1.f }, { 24.f, -24.f }, { 600.f, 60.f }));
        lJumps->SetFont(kFace);
        lJumps->SetSize(40.f);
        lJumps->SetText("Jumps: 0");
        m_Jumps = static_cast<UIText*>(m_Panel->AddChild(std::move(lJumps)));

        auto lTrack = MakeUnique<UIImage>();
        lTrack->Name = "SpeedTrack";
        lTrack->SetRect(Corner({ 0.f, 0.f }, { 24.f, 24.f }, { 320.f, 28.f }));
        lTrack->SetColor({ 0.08f, 0.08f, 0.1f, 0.85f });
        UIWidget* lTrackWidget = m_Panel->AddChild(std::move(lTrack));

        auto lFill = MakeUnique<UIImage>();
        lFill->Name = "SpeedFill";
        lFill->SetRect(Stretched());
        lFill->SetColor({ 0.2f, 0.85f, 0.35f, 1.f });
        lFill->SetFill(EUIFill::Horizontal, 0.f);
        m_Speed = static_cast<UIImage*>(lTrackWidget->AddChild(std::move(lFill)));

        if (m_Context->Actions != nullptr)
        {
            m_Context->Actions->Bind(kJumpAction, EInputTrigger::Started, this, &HudSubsystem::OnJump);
        }

        OPAAX_LOG(LogHud, Info, "HUD started — {} widget(s) under '{}', face '{}'",
                  4, m_Panel->Name.CStr(), kFace);
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
        if (m_Panel != nullptr && m_Context->UI != nullptr)
        {
            m_Context->UI->GetCanvas().Root().RemoveChild(*m_Panel);
        }

        m_Panel = nullptr;
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
