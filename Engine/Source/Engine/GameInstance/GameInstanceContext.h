#pragma once

#include "Application/Services/ICommandRegistry.h"
#include "Application/Services/IJobSystem.h"
#include "Application/Services/IOnlineServices.h"
#include "Application/Services/ISaveStorage.h"
#include "Application/Services/IUserSettings.h"

namespace Opaax
{
    class WorldManager;
    class ResourceManager;
    class EngineEventBus;
    class InputManager;
    class IPaths;
    struct EngineConfigData;

    // =============================================================================
    // GameInstanceContext — what a game gives each of its subsystems (GameInstanceSubsystem::GetContext).
    //   One per game, valid for the whole game.
    // =============================================================================
    struct GameInstanceContext
    {
        /**
         * Every world, and the active one. A game outlives its worlds, so never cache a World.
         */
        WorldManager& Worlds;

        /** Resources, shared by every world and game. */
        ResourceManager& Resources;

        /**
         * Asset-relative -> absolute paths (needed by ResourceManager::Load).
         */
        const IPaths& Paths;

        /** The engine event bus. */
        EngineEventBus& Events;

        /**
         * This frame's raw keyboard and mouse state.
         */
        const InputManager& Input;

        /**
         * The engine config (read-only).
         */
        const EngineConfigData& Config;

        /** The worker threads, for work that must not stall a frame (a save). The null one runs each job at once. */
        IJobSystem& Jobs = IJobSystem::Null();

        /** The player's save slots. The null one keeps nothing. */
        ISaveStorage& Saves = ISaveStorage::Null();

        /**
         * The player's settings: a settings menu sets them, Config shows what the engine reads. Over the
         * configs for the whole game. The null one refuses every change.
         */
        IUserSettings& Settings = IUserSettings::Null();

        /** The player's store (leaderboards, achievements, the overlay). With none, requests answer Unavailable. */
        IOnlineServices& Online = IOnlineServices::Null();

        /**
         * The app's commands (automation, the console). Keep what Register returns as a member: the
         * command is gone with it. The null one keeps nothing.
         */
        ICommandRegistry& Commands = ICommandRegistry::Null();
    };
}
