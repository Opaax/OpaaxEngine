#include "Systems/HudSubsystem.h"

#include <algorithm>

#include <glm/geometric.hpp>   // glm::length

#include "Core/Log/Logger.h"
#include "Engine/Input/InputMappingSubsystem.h"
#include "Engine/Input/InputTypes.h"
#include "Engine/UI/UISubsystem.h"
#include "UI/UIBinding.h"
#include "UI/UICanvas.h"
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

        /** The authored tree, and the source name its bindings pull from. */
        constexpr const char* kHudAsset  = "UI/Hud.opaaxui";
        const OpaaxStringID   kHudSource = OPAAX_ID("Hud");

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

        // The tenant loads, checks the height against its canvas and hangs it (UI13, UI2).
        m_Root = m_Context->UI->MountAsset(OpaaxString(kHudAsset));
        if (m_Root == nullptr)
        {
            OPAAX_LOG(LogHud, Error, "HUD asset '{}' did not mount — no HUD is drawn.", kHudAsset);
            return true;
        }

        // The model is the ONE thing this owns that the tree reads; which widget shows which field
        // is the asset's business (UI24). Removed BY HANDLE in Shutdown, before the model dies:
        // during a level swap the next world's HUD has already re-registered "Hud", and a remove
        // by name would take its source away.
        UICanvas& lCanvas = m_Context->UI->GetCanvas();
        m_Source = lCanvas.Bindings().Add(kHudSource, MakeBindingReader(m_Model));

        if (m_Context->Actions != nullptr)
        {
            m_Context->Actions->Bind(kJumpAction, EInputTrigger::Started, this, &HudSubsystem::OnJump);
        }

        OPAAX_LOG(LogHud, Info, "HUD up — source '{}' registered ({} sources on the canvas)",
                  kHudSource, lCanvas.Bindings().Count());
        return true;
    }

    void HudSubsystem::Update(double)
    {
        float lFastest = 0.f;
        m_Context->OwningWorld.Each<MoverComponent>([&lFastest](EntityID, const MoverComponent& InMover)
        {
            lFastest = std::max(lFastest, glm::length(InMover.Velocity));
        });

        m_Model.Speed = std::clamp(lFastest / kFullSpeed, 0.f, 1.f);
    }

    void HudSubsystem::Shutdown()
    {
        if (m_Context->Actions != nullptr)
        {
            m_Context->Actions->UnbindAll(this);
        }

        // The canvas outlives this world (GI1); the HUD and its source do not.
        if (m_Context->UI != nullptr)
        {
            m_Context->UI->GetCanvas().Bindings().Remove(m_Source);

            if (m_Root != nullptr)
            {
                m_Context->UI->GetCanvas().Root().RemoveChild(*m_Root);
            }
        }

        m_Root   = nullptr;
        m_Source = {};
    }

    void HudSubsystem::OnJump(const InputActionValue& /*InValue*/)
    {
        // Bindings outlive worlds (IM8): the outgoing world's HUD hears this too.
        if (!m_Context->OwningWorld.IsActive())
        {
            return;
        }

        ++m_Model.Jumps;
    }
}
