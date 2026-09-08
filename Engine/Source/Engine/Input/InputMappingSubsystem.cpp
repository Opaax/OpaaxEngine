#include "Engine/Input/InputMappingSubsystem.h"

#include "Engine/GameInstance/GameInstanceContext.h"
#include "World/WorldManager.h"

namespace Opaax
{
    InputMappingSubsystem::InputMappingSubsystem(GameInstanceContext& InContext) noexcept
        : m_Context(&InContext)
    {
    }

    bool InputMappingSubsystem::Startup()
    {
        // The PLAY world count is the GATE, not decoration: a game is started before any world
        // that belongs to it, so anything other than 0 here means the session was created in
        // reaction to a world and every WorldContext built before this point missed it.
        //
        // PLAY worlds specifically, not all of them — the editor starts a game while its EDIT
        // world is on screen, so a total count reads 1 there and cannot tell a correct boot from
        // a broken one. Total is printed alongside only as context.
        OPAAX_LOG(LogInputMapping, Info,
                  "Input mapping started before any play world exists ({} play world(s), {} total) — {} context(s) mapped",
                  m_Context->Worlds.CountWorldsOfMode(EWorldMode::Play),
                  m_Context->Worlds.GetWorldCount(), m_ContextCount);
        return true;
    }

    void InputMappingSubsystem::Shutdown()
    {
        OPAAX_LOG(LogInputMapping, Info, "Input mapping shutdown ({} context(s) released)", m_ContextCount);
    }
}
