#pragma once

#include <nlohmann/json.hpp>

#include "Application/Services/ICommandRegistry.h"
#include "Commands/CommandHandle.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxStringJson.h"
#include "World/Systems/WorldContext.h"
#include "World/Systems/WorldSubsystem.h"
#include "World/World.h"
#include "World/WorldSpec.h"

namespace TestWorld
{
    // =============================================================================
    // CommandProbeSubsystem — a world subsystem of the Commands level's Play worlds: registers
    //   probe.ping for the world's lifetime. probe.ping answers {"world": its name, "pings": how many
    //   times this world was pinged}; in another level it is an unknown command.
    // =============================================================================
    class CommandProbeSubsystem final : public Opaax::WorldSubsystem
    {
        OPAAX_OBJECT(CommandProbeSubsystem, Opaax::WorldSubsystem)

        bool ShouldCreate(const Opaax::World& InWorld) const override
        {
            return InWorld.GetMode() == Opaax::EWorldMode::Play && InWorld.GetName() == Opaax::OpaaxStringID("Commands");
        }

        bool Startup() override
        {
            if (GetContext().Commands == nullptr)
            {
                return true;
            }

            m_Ping = GetContext().Commands->Register(
                Opaax::CommandSpec("probe.ping", "TestWorld: answers {world, pings} in the Commands level."),
                [this](const nlohmann::json&)
                {
                    ++m_Pings;
                    return Opaax::CommandResult::Ok(nlohmann::json{
                        { "world", GetContext().OwningWorld.GetName().ToString() }, { "pings", m_Pings } });
                });
            return true;
        }

    private:
        Opaax::CommandHandle m_Ping;
        Opaax::Int32         m_Pings = 0;
    };
}
