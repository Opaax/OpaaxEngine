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
#include "UI/UIBinding.h"
#include "UI/UICanvas.h"
#include "UI/UICanvasFile.h"
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

        // The model is the ONE thing this owns that the tree reads; which widget shows which field
        // is the asset's business (UI24). Removed in Shutdown, before the model dies.
        lCanvas.Bindings().Add(kHudSource, MakeBindingReader(m_Model));

        if (m_Context->Actions != nullptr)
        {
            m_Context->Actions->Bind(kJumpAction, EInputTrigger::Started, this, &HudSubsystem::OnJump);
        }

        OPAAX_LOG(LogHud, Info, "HUD loaded '{}' — {} widget(s), reference height {}, source '{}' registered ({} sources on the canvas)",
                  kHudAsset, UICanvasFile::CountWidgets(*m_Root), lReferenceHeight, kHudSource, lCanvas.Bindings().Count());
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
            m_Context->UI->GetCanvas().Bindings().Remove(kHudSource);

            if (m_Root != nullptr)
            {
                m_Context->UI->GetCanvas().Root().RemoveChild(*m_Root);
            }
        }

        m_Root = nullptr;

        OPAAX_LOG(LogHud, Info, "HUD shutdown — {} jump(s) counted", m_Model.Jumps);
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
