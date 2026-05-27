#include "PlayerDemoSystem.h"

#include <cmath>

#include "Core/CoreEngineApp.h"
#include "Core/Event/OpaaxEventDispatcher.hpp"
#include "Core/Event/OpaaxEventTypes.hpp"
#include "Core/Input/OpaaxInputEvents.hpp"
#include "Core/Log/OpaaxLog.h"
#include "Core/OpaaxStringID.hpp"
#include "ECS/Components/TagComponent.h"
#include "ECS/Components/TransformComponent.h"
#include "Renderer/Camera/CameraControllerSystem.h"
#include "Renderer/Camera/FollowCameraController.h"
#include "Renderer/Camera/FollowParams.h"
#include "Renderer/Camera/ShakeParams.h"
#include "World/World.h"

void PlayerDemoSystem::Update(double DeltaTime)
{
    m_DemoTime += static_cast<float>(DeltaTime);

    Opaax::World& lWorld = GetEngineApp()->GetWorld();

    // Find the Player entity by tag — re-lookup every frame because PIE restart rebuilds
    // the world and assigns a fresh EntityID.
    Opaax::EntityID lPlayer = Opaax::ENTITY_NONE;
    lWorld.Each<Opaax::ECS::TagComponent>(
        [&](Opaax::EntityID InEntity, Opaax::ECS::TagComponent& InTag)
        {
            if (InTag.Tag == OPAAX_ID("Player"))
            {
                lPlayer = InEntity;
            }
        });

    if (lPlayer == Opaax::ENTITY_NONE)
    {
        return;
    }

    // Re-attach the follow controller when the Player's EntityID changes (first frame,
    // or after a PIE restart that rebuilt the world).
    if (lPlayer != m_PlayerEntity)
    {
        m_PlayerEntity = lPlayer;

        if (auto* lCtl = GetEngineApp()->GetGameSubsystem<Opaax::CameraControllerSystem>())
        {
            lCtl->DetachAll();
            lCtl->AttachController(
                Opaax::MakeUnique<Opaax::FollowCameraController>(
                    Opaax::FollowParams{ lPlayer, { 0.f, 0.f }, 0.3f, { 0.f, 0.f } },
                    lWorld));
        }
    }

    // Drive the Player on a Lissajous curve so the follow camera has motion to track.
    if (auto* lTr = lWorld.GetComponent<Opaax::ECS::TransformComponent>(m_PlayerEntity))
    {
        lTr->Position.x = std::sin(m_DemoTime)        * 200.f;
        lTr->Position.y = std::cos(m_DemoTime * 0.7f) * 100.f;
    }
}

Opaax::Uint32 PlayerDemoSystem::GetEventCategoryFilter() const noexcept
{
    return Opaax::EEventCategory_Keyboard;
}

bool PlayerDemoSystem::OnEvent(Opaax::OpaaxEvent& InEvent)
{
    Opaax::OpaaxEventDispatcher lDispatcher(InEvent);

    lDispatcher.Dispatch<Opaax::KeyPressedEvent>([this](Opaax::KeyPressedEvent& E) -> bool
    {
        if (E.IsRepeat())                        return false;
        if (E.GetKeyCode() != Opaax::EOpaaxKeyCode::K) return false;

        if (auto* lCtl = GetEngineApp()->GetGameSubsystem<Opaax::CameraControllerSystem>())
        {
            OPAAX_CORE_INFO("PlayerDemoSystem - K pressed: triggering camera shake.");
            lCtl->TriggerShake(Opaax::ShakeParams{
                /*Amplitude*/       20.f,
                /*Frequency*/       30.f,
                /*Duration*/        0.4f,
                /*Decay*/           Opaax::EShakeDecay::EaseOut,
                /*YFrequencyRatio*/ 1.3f,
                /*YPhase*/          1.7f });
        }
        return false;
    });

    return false;
}
