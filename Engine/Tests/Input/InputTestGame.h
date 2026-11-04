// InputTestGame.h — a game's input mapping over context asset files in a temp directory: the
//   services a GameInstanceContext names, a walking context and a locked menu as files, and the
//   player's settings in a save folder of their own. Shared by the input and automation suites.
#pragma once

#include <doctest.h>

#include "../TestFolderPaths.h"
#include "../TestTempDir.h"

#include "Application/Services/IConfigSystem.h"
#include "Application/Services/ISaveStorage.h"
#include "Application/Services/IUserSettings.h"
#include "Application/Services/IWindowManager.h"
#include "Engine/Config/EngineConfigData.h"
#include "Engine/GameInstance/GameInstance.h"
#include "Engine/GameInstance/GameInstanceContext.h"
#include "Engine/Subsystems/EngineEventBus.h"
#include "Input/Assets/InputActionAsset.h"
#include "Input/Assets/InputMappingContextAsset.h"
#include "Input/InputManager.h"
#include "Input/Mapping/InputMappingSubsystem.h"
#include "Object/ObjectClassRegistry.h"
#include "Resources/Asset/AssetFile.h"
#include "Resources/ResourceManager.h"
#include "World/WorldManager.h"

namespace InputTesting
{
    using namespace Opaax;

    struct InputClasses final : ObjectClassRegistry
    {
        InputClasses()
        {
            Register<InputActionAsset>();
            Register<InputMappingContextAsset>();
        }
    };

    inline InputMappingEntry Mapping(const char* InActionPath, const EKeyCode InKey)
    {
        InputMappingEntry lEntry;
        lEntry.Action.Path = OpaaxString(InActionPath);
        lEntry.Key         = InKey;
        return lEntry;
    }

    /** The engine-side half of a GameInstanceContext, its assets under InRoot. */
    struct ContextFixture
    {
        explicit ContextFixture(const OpaaxString& InRoot, const ObjectClassRegistry& InClasses)
            : Paths(InRoot)
        {
            Resources.SetObjectClasses(&InClasses);
            REQUIRE(Resources.Startup());
        }

        ~ContextFixture() { Resources.Shutdown(); }

        GameInstanceContext Make()
        {
            return GameInstanceContext{ Worlds, Resources, Paths, Events, Input, Config, IJobSystem::Null(),
                                        ISaveStorage::Null(), Settings != nullptr ? *Settings : IUserSettings::Null() };
        }

        /** The game's input mapping, made and started the way a game makes it, with this context. */
        InputMappingSubsystem& StartMapping()
        {
            Game = NewObject<GameInstance>();
            Game->Init(Make());
            Game->GetSubsystems().Add<InputMappingSubsystem>();
            Game->GetSubsystems().StartupAll();
            return *Game->GetSubsystems().GetSubsystem<InputMappingSubsystem>();
        }

        /** Presses InKey and runs one frame of InMapping. */
        void Press(InputMappingSubsystem& InMapping, const EKeyCode InKey)
        {
            Input.OnKeyPressed(InKey, false);
            InMapping.Update(1.0 / 60.0);
            Input.EndFrame();
        }

        /** Releases InKey and runs one frame of InMapping. */
        void Release(InputMappingSubsystem& InMapping, const EKeyCode InKey)
        {
            Input.OnKeyReleased(InKey);
            InMapping.Update(1.0 / 60.0);
            Input.EndFrame();
        }

        /** Whether InKey drives InAction: pressed, read, released. */
        bool Drives(InputMappingSubsystem& InMapping, const EKeyCode InKey, const char* InAction)
        {
            Press(InMapping, InKey);
            const bool bDriven = InMapping.GetValue(OpaaxStringID(OpaaxString(InAction))).AsBool();
            Release(InMapping, InKey);
            return bDriven;
        }

        /** The player's settings the game is given; none (the null ones) when unset. Outlive the game. */
        IUserSettings* Settings = nullptr;

        TestFolderPaths  Paths;
        WorldManager     Worlds;
        ResourceManager  Resources;
        EngineEventBus   Events;
        InputManager     Input;
        EngineConfigData Config;

        // Last: the game ends before the services its context names.
        TStrongObjectPtr<GameInstance> Game;
    };

    // A walking context (Jump on Space, Use on E) and a locked menu (Pause on Escape), as files.
    inline void WriteControls(const TestTempDir& InDir)
    {
        REQUIRE(AssetFile::Save(InDir.Sub("Input/Jump.opaaxasset"), InputActionAsset{}));
        REQUIRE(AssetFile::Save(InDir.Sub("Input/Use.opaaxasset"), InputActionAsset{}));
        REQUIRE(AssetFile::Save(InDir.Sub("Input/Pause.opaaxasset"), InputActionAsset{}));

        InputMappingContextAsset lControls;
        lControls.Mappings = { Mapping("Input/Jump.opaaxasset", EKeyCode::Space), Mapping("Input/Use.opaaxasset", EKeyCode::E) };
        REQUIRE(AssetFile::Save(InDir.Sub("Input/Controls.opaaxasset"), lControls));

        InputMappingContextAsset lMenu;
        lMenu.Priority    = 10;
        lMenu.bRebindable = false;
        lMenu.Mappings    = { Mapping("Input/Pause.opaaxasset", EKeyCode::Escape) };
        REQUIRE(AssetFile::Save(InDir.Sub("Input/Menu.opaaxasset"), lMenu));
    }

    inline const OpaaxString CONTROLS_PATH("Input/Controls.opaaxasset");
    inline const OpaaxString MENU_PATH("Input/Menu.opaaxasset");

    inline InputKeySlot JumpSlot() { return InputKeySlot{ CONTROLS_PATH, OPAAX_ID("Jump"), EKeyCode::Space }; }
    inline InputKeySlot UseSlot()  { return InputKeySlot{ CONTROLS_PATH, OPAAX_ID("Use"), EKeyCode::E }; }

    /** A player's settings in a save folder of their own, over no config. */
    struct PlayerSettings
    {
        PlayerSettings(const TestTempDir& InDir, const IPaths& InPaths)
            : Storage(InDir.Sub("SaveGames")), Configs(InPaths) {}

        /** As a game starts: read, and over the configs. */
        TUniquePtr<UserSettings> Launch()
        {
            TUniquePtr<UserSettings> lSettings = MakeUnique<UserSettings>(Storage, Configs, IWindowManager::Null());
            lSettings->Load();
            lSettings->Activate();
            return lSettings;
        }

        SaveStorage  Storage;
        ConfigSystem Configs;
    };
}
