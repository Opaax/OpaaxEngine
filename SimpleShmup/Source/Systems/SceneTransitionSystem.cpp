#include "SceneTransitionSystem.h"

#include "Core/CoreEngineApp.h"
#include "Core/ApplicationEvents.hpp"
#include "Core/Event/OpaaxEventDispatcher.hpp"
#include "Core/Event/OpaaxEventTypes.hpp"
#include "Core/Input/OpaaxInputEvents.hpp"
#include "Core/Input/OpaaxInputTypes.hpp"
#include "Core/Log/OpaaxLog.h"
#include "Scene/SceneManager.h"

#include "Scene/MenuScene.h"
#include "Scene/ShmupGameScene.h"

using namespace Opaax;

Uint32 SceneTransitionSystem::GetEventCategoryFilter() const noexcept
{
    return EEventCategory_Keyboard | EEventCategory_Application;
}

bool SceneTransitionSystem::OnEvent(OpaaxEvent& Event)
{
    OpaaxEventDispatcher lDispatcher(Event);

    lDispatcher.Dispatch<KeyPressedEvent>([this](KeyPressedEvent& E) -> bool
    {
        if (E.IsRepeat()) return false;

        switch (E.GetKeyCode())
        {
            case EOpaaxKeyCode::Space:  m_bSpaceJustPressed = true; break;
            case EOpaaxKeyCode::Escape: m_bEscHeld          = true; break;
            default: break;
        }
        return false;
    });

    lDispatcher.Dispatch<KeyReleasedEvent>([this](KeyReleasedEvent& E) -> bool
    {
        if (E.GetKeyCode() == EOpaaxKeyCode::Escape)
        {
            m_bEscHeld   = false;
            m_EscHeldFor = 0.f;
        }
        return false;
    });

    lDispatcher.Dispatch<WindowLostFocusEvent>([this](WindowLostFocusEvent&) -> bool
    {
        m_bSpaceJustPressed = false;
        m_bEscHeld          = false;
        m_EscHeldFor        = 0.f;
        return false;
    });

    return false;
}

void SceneTransitionSystem::Update(double DeltaTime)
{
    SceneManager* lSceneMgr = GetEngineApp()->GetSceneManager();
    if (!lSceneMgr) return;

    Scene* lActive = lSceneMgr->GetActiveScene();
    if (!lActive)
    {
        m_bSpaceJustPressed = false;
        return;
    }

    const OpaaxString& lName = lActive->GetName();

    if (lName == "Menu")
    {
        if (m_bSpaceJustPressed)
        {
            OPAAX_TRACE("[SceneTransitionSystem] Menu → ShmupGame");
            lSceneMgr->Replace(MakeUnique<ShmupGameScene>());
            // Pointer above is invalid past this point — bail.
            m_bSpaceJustPressed = false;
            return;
        }
    }
    else if (lName == "ShmupGame")
    {
        if (m_bEscHeld)
        {
            m_EscHeldFor += static_cast<float>(DeltaTime);
            if (m_EscHeldFor >= EscHoldThreshold)
            {
                OPAAX_TRACE("[SceneTransitionSystem] ShmupGame → Menu (Esc held {:.2f}s)", m_EscHeldFor);
                lSceneMgr->Replace(MakeUnique<MenuScene>());
                m_bEscHeld          = false;
                m_EscHeldFor        = 0.f;
                m_bSpaceJustPressed = false;
                return;
            }
        }
    }

    // Edge-trigger: consume Space regardless of which scene is active so it
    // can't fire stale on the next frame after a transition.
    m_bSpaceJustPressed = false;
}
