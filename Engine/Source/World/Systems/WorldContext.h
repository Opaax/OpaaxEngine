#pragma once

namespace Opaax
{
    class World;
    class AudioManager;
    class GameInstance;
    class ResourceManager;
    class EngineEventBus;
    class DebugDraw;
    class IPaths;
    class InputManager;
    class InputMappingSubsystem;
    class UISubsystem;
    class ComponentRegistry;
    struct EngineConfigData;

    // =============================================================================
    // WorldContext — references given to each world subsystem at construction (no service locator).
    //   One per world, owned by it: a subsystem may keep a reference for the world's lifetime.
    //   Add a member when a subsystem needs it (and add it to WorldManager::CreateSubsystemsFor's
    //   null check).
    // =============================================================================
    struct WorldContext
    {
        /** The world this subsystem belongs to. */
        World& OwningWorld;

        /** Resources, shared by every world. */
        ResourceManager& Resources;

        /**
         * Asset-relative -> absolute paths (needed by ResourceManager::Load).
         */
        const IPaths& Paths;

        /** The engine event bus. */
        EngineEventBus& Events;

        /**
         * This frame's raw input (read-only).
         */
        const InputManager& Input;

        /**
         * The engine config (read-only).
         */
        const EngineConfigData& Config;

        /**
         * This frame's input actions. Null when no game is running (Edit worlds): check before use,
         * except in Play-only subsystems.
         */
        InputMappingSubsystem* Actions;

        /** The game's UI canvas. Null when no game is running. */
        UISubsystem* UI;

        /**
         * Debug shapes, cleared every frame: submit every frame. The only way a world subsystem draws.
         */
        DebugDraw& Debug;

        /** Every registered component and behaviour type. Null in a bare test world. */
        const ComponentRegistry* Components = nullptr;

        /** The sound mixer. Null in a bare test world. */
        AudioManager* Audio = nullptr;

        /** The running game, whose subsystems last across levels. Null when no game is running (Edit worlds). */
        GameInstance* Game = nullptr;
    };
}
