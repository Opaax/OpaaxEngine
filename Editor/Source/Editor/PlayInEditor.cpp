#include "Editor/PlayInEditor.h"

#include "Application/Services/ILogger.h"   // OPAAX_LOG + LogCategory
#include "World/World.h"
#include "World/WorldManager.h"

using namespace Opaax;   // OPAAX_LOG expands to an unqualified ToSpdLevel(...)

namespace
{
    constexpr LogCategory LogPlayInEditor{"PlayInEditor"};
}

namespace Opaax::Editor
{
    const char* ToString(EPlayState InState) noexcept
    {
        switch (InState)
        {
        case EPlayState::Edit:    return "Edit";
        case EPlayState::Playing: return "Playing";
        case EPlayState::Paused:  return "Paused";
        }

        return "Unknown";
    }

    bool PlayInEditor::Play()
    {
        if (m_State != EPlayState::Edit)
        {
            OPAAX_LOG(LogPlayInEditor, Warn, "Play refused — already {}.", ToString(m_State))
            return false;
        }

        World* lEditWorld = m_Worlds.GetActiveWorld();
        if (lEditWorld == nullptr)
        {
            OPAAX_LOG(LogPlayInEditor, Error, "Play refused — there is no active world to clone.")
            return false;
        }

        World* lPlayWorld = m_Worlds.CloneWorld(*lEditWorld, EWorldMode::Play);
        if (lPlayWorld == nullptr)
        {
            // CloneWorld already logged why. Nothing was created, so the state stays Edit and the
            // editor keeps showing the world it was showing.
            return false;
        }

        m_EditWorld = lEditWorld;
        m_PlayWorld = lPlayWorld;

        // The edit world is NOT destroyed or modified — it simply stops being the active one. That
        // is the entire restore mechanism (Editor.md D6).
        m_Worlds.SetActiveWorld(lPlayWorld);
        m_Worlds.SetPaused(false);

        m_State = EPlayState::Playing;

        OPAAX_LOG(LogPlayInEditor, Info, "PLAY — edit world '{}' cloned into a Play world ({} entities)",
                  m_EditWorld->GetName().CStr(), lPlayWorld->GetEntityCount())
        return true;
    }

    bool PlayInEditor::Pause()
    {
        if (m_State != EPlayState::Playing)
        {
            OPAAX_LOG(LogPlayInEditor, Warn, "Pause refused — state is {}.", ToString(m_State))
            return false;
        }

        m_Worlds.SetPaused(true);
        m_State = EPlayState::Paused;

        OPAAX_LOG(LogPlayInEditor, Info, "PAUSE")
        return true;
    }

    bool PlayInEditor::Resume()
    {
        if (m_State != EPlayState::Paused)
        {
            OPAAX_LOG(LogPlayInEditor, Warn, "Resume refused — state is {}.", ToString(m_State))
            return false;
        }

        m_Worlds.SetPaused(false);
        m_State = EPlayState::Playing;

        OPAAX_LOG(LogPlayInEditor, Info, "RESUME")
        return true;
    }

    bool PlayInEditor::TogglePause()
    {
        return m_State == EPlayState::Paused ? Resume() : Pause();
    }

    bool PlayInEditor::Step()
    {
        if (m_State != EPlayState::Paused)
        {
            OPAAX_LOG(LogPlayInEditor, Warn, "Step refused — only a PAUSED session can step (state is {}).",
                      ToString(m_State))
            return false;
        }

        m_Worlds.RequestStep();

        OPAAX_LOG(LogPlayInEditor, Info, "STEP — one frame")
        return true;
    }

    bool PlayInEditor::Stop()
    {
        if (m_State == EPlayState::Edit)
        {
            OPAAX_LOG(LogPlayInEditor, Warn, "Stop refused — no play session is running.")
            return false;
        }

        // Re-activate BEFORE destroying: DestroyWorld clears the active slot when it is destroying
        // the active world, which would leave the editor with no world for the rest of the frame.
        if (m_EditWorld != nullptr)
        {
            m_Worlds.SetActiveWorld(m_EditWorld);
        }

        if (m_PlayWorld != nullptr)
        {
            m_Worlds.DestroyWorld(m_PlayWorld);
        }

        const OpaaxString lRestored = m_EditWorld != nullptr ? m_EditWorld->GetName() : OpaaxString("<none>");

        m_EditWorld = nullptr;
        m_PlayWorld = nullptr;

        m_Worlds.SetPaused(false);   // a Stop while paused must not leave the edit world frozen
        m_State = EPlayState::Edit;

        OPAAX_LOG(LogPlayInEditor, Info, "STOP — edit world '{}' restored, play clone destroyed", lRestored.CStr())
        return true;
    }
}
