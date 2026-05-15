#include "PlayerControlSystem.h"

#include <cmath>

#include "Core/CoreEngineApp.h"
#include "Core/ApplicationEvents.hpp"
#include "Core/Config/EngineConfig.h"
#include "Core/Event/OpaaxEventDispatcher.hpp"
#include "Core/Event/OpaaxEventTypes.hpp"
#include "Core/Input/OpaaxInputEvents.hpp"

#include "World/World.h"

#include "ECS/Components/TransformComponent.h"
#include "ECS/Components/VelocityComponent.h"
#include "ECS/Components/LifetimeComponent.h"
#include "ECS/Components/AABB2DComponent.h"
#include "ECS/Components/PlayerTagComponent.h"
#include "ECS/Components/BulletTagComponent.h"

using namespace Opaax;

Uint32 PlayerControlSystem::GetEventCategoryFilter() const noexcept
{
    return EEventCategory_Keyboard | EEventCategory_Application;
}

bool PlayerControlSystem::OnEvent(OpaaxEvent& Event)
{
    OpaaxEventDispatcher lDispatcher(Event);

    lDispatcher.Dispatch<KeyPressedEvent>([this](KeyPressedEvent& E) -> bool
    {
        if (E.IsRepeat()) return false;
        HandleKeyDown(E.GetKeyCode());
        return false;
    });

    lDispatcher.Dispatch<KeyReleasedEvent>([this](KeyReleasedEvent& E) -> bool
    {
        HandleKeyUp(E.GetKeyCode());
        return false;
    });

    lDispatcher.Dispatch<WindowLostFocusEvent>([this](WindowLostFocusEvent&) -> bool
    {
        ResetAllInput();
        return false;
    });

    return false;
}

void PlayerControlSystem::HandleKeyDown(EOpaaxKeyCode InKey)
{
    switch (InKey)
    {
        case EOpaaxKeyCode::W:     m_bW     = true; break;
        case EOpaaxKeyCode::A:     m_bA     = true; break;
        case EOpaaxKeyCode::S:     m_bS     = true; break;
        case EOpaaxKeyCode::D:     m_bD     = true; break;
        case EOpaaxKeyCode::Up:    m_bUp    = true; break;
        case EOpaaxKeyCode::Down:  m_bDown  = true; break;
        case EOpaaxKeyCode::Left:  m_bLeft  = true; break;
        case EOpaaxKeyCode::Right: m_bRight = true; break;
        case EOpaaxKeyCode::Space:
            m_bSpaceHeld = true;
            // First shot fires immediately on next Update.
            m_FireTimer  = 1.f / FireRateHz;
            break;
        default: break;
    }
}

void PlayerControlSystem::HandleKeyUp(EOpaaxKeyCode InKey)
{
    switch (InKey)
    {
        case EOpaaxKeyCode::W:     m_bW     = false; break;
        case EOpaaxKeyCode::A:     m_bA     = false; break;
        case EOpaaxKeyCode::S:     m_bS     = false; break;
        case EOpaaxKeyCode::D:     m_bD     = false; break;
        case EOpaaxKeyCode::Up:    m_bUp    = false; break;
        case EOpaaxKeyCode::Down:  m_bDown  = false; break;
        case EOpaaxKeyCode::Left:  m_bLeft  = false; break;
        case EOpaaxKeyCode::Right: m_bRight = false; break;
        case EOpaaxKeyCode::Space:
            m_bSpaceHeld = false;
            m_FireTimer  = 0.f;
            break;
        default: break;
    }
}

void PlayerControlSystem::ResetAllInput()
{
    m_bW = m_bA = m_bS = m_bD = false;
    m_bUp = m_bDown = m_bLeft = m_bRight = false;
    m_bSpaceHeld = false;
    m_FireTimer  = 0.f;
}

void PlayerControlSystem::Update(double DeltaTime)
{
    World& lWorld = GetEngineApp()->GetWorld();
    const float lDt = static_cast<float>(DeltaTime);

    // Combine WASD + arrows — pressing either side latches that direction.
    const bool bWantUp    = m_bW || m_bUp;
    const bool bWantDown  = m_bS || m_bDown;
    const bool bWantLeft  = m_bA || m_bLeft;
    const bool bWantRight = m_bD || m_bRight;

    Vector2F lDir{
        (bWantRight ? 1.f : 0.f) - (bWantLeft ? 1.f : 0.f),
        (bWantUp    ? 1.f : 0.f) - (bWantDown ? 1.f : 0.f)
    };

    const float lLenSq = lDir.x * lDir.x + lDir.y * lDir.y;
    if (lLenSq > 0.f)
    {
        const float lInvLen = 1.f / std::sqrt(lLenSq);
        lDir.x *= lInvLen;
        lDir.y *= lInvLen;
    }

    // NOTE: single-player assumption — first match wins.
    Vector2F lPlayerPos{ 0.f, 0.f };
    bool     bFoundPlayer = false;

    const float lMaxX = static_cast<float>(EngineConfig::WindowWidth())  * 0.5f - 16.f;
    const float lMaxY = static_cast<float>(EngineConfig::WindowHeight()) * 0.5f - 16.f;

    lWorld.Each<const PlayerTagComponent, Opaax::ECS::TransformComponent, VelocityComponent>(
        [&](EntityID, const PlayerTagComponent&, Opaax::ECS::TransformComponent& InTransform, VelocityComponent& InVelocity)
        {
            InVelocity.Velocity.x = lDir.x * PlayerSpeed;
            InVelocity.Velocity.y = lDir.y * PlayerSpeed;

            // Clamp post-integration position. Movement integration runs after this
            // system in tick order, so this clamps last frame's drift; next frame's
            // movement is bounded by the same clamp on its way back through.
            if (InTransform.Position.x >  lMaxX) InTransform.Position.x =  lMaxX;
            if (InTransform.Position.x < -lMaxX) InTransform.Position.x = -lMaxX;
            if (InTransform.Position.y >  lMaxY) InTransform.Position.y =  lMaxY;
            if (InTransform.Position.y < -lMaxY) InTransform.Position.y = -lMaxY;

            lPlayerPos   = InTransform.Position;
            bFoundPlayer = true;
        });

    if (!bFoundPlayer) return;

    if (m_bSpaceHeld)
    {
        m_FireTimer += lDt;
        const float lStep = 1.f / FireRateHz;
        while (m_FireTimer >= lStep)
        {
            m_FireTimer -= lStep;

            EntityID lBullet = lWorld.CreateEntity("Bullet");

            Opaax::ECS::TransformComponent lTransform;
            lTransform.Position = lPlayerPos;
            lWorld.AddComponent<Opaax::ECS::TransformComponent>(lBullet, lTransform);

            VelocityComponent lVel;
            lVel.Velocity = { BulletSpeed, 0.f };
            lWorld.AddComponent<VelocityComponent>(lBullet, lVel);

            LifetimeComponent lLife;
            lLife.SecondsRemaining = BulletLifetime;
            lWorld.AddComponent<LifetimeComponent>(lBullet, lLife);

            AABB2DComponent lAabb;
            lAabb.HalfExtents = { 4.f, 2.f };
            lWorld.AddComponent<AABB2DComponent>(lBullet, lAabb);

            lWorld.AddComponent<BulletTagComponent>(lBullet);
        }
    }
}
