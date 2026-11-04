// Suite: the input assets — InputActionAsset and InputMappingContextAsset — through their files, the
// conversion of the old .opaaxaction and .opaaxinputmap formats, and AddContextAsset, which turns them
// into the evaluator's actions and bindings. File cases use a temp directory of their own.
#include <doctest.h>

#include "../LegacyAssetFile.h"
#include "../TestTempDir.h"

#include "Application/Services/IConfigSystem.h"
#include "Application/Services/IPaths.h"
#include "Application/Services/ISaveStorage.h"
#include "Application/Services/IUserSettings.h"
#include "Application/Services/IWindowManager.h"
#include "Core/String/OpaaxPathString.h"
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

using namespace Opaax;

namespace
{
    struct InputClasses final : ObjectClassRegistry
    {
        InputClasses()
        {
            Register<InputActionAsset>();
            Register<InputMappingContextAsset>();
        }
    };

    // Every asset under one folder, so AssetToAbsolute is a concatenation. AddContextAsset reaches
    // for that one resolver; the rest of IPaths answers the folder.
    class FolderPaths final : public IPaths
    {
    public:
        explicit FolderPaths(const OpaaxString& InRoot) : m_Root(InRoot) {}

        OpaaxString AssetToAbsolute(const OpaaxString& InAssetRel) const override
        {
            return PathString::Join(m_Root, InAssetRel);
        }

        OpaaxString AbsoluteToAsset(const OpaaxString& InAbsPath) const override
        {
            return PathString::RelativeTo(InAbsPath, m_Root);
        }

        void        LogPaths()      const override {}
        OpaaxString WorkspaceRoot() const override { return m_Root; }
        OpaaxString EngineRoot()    const override { return m_Root; }
        OpaaxString ProjectRoot()   const override { return m_Root; }
        OpaaxString ProjectFile()   const override { return m_Root; }
        OpaaxString AssetsDir()     const override { return m_Root; }
        OpaaxString ConfigsDir()    const override { return m_Root; }
        OpaaxString SourceDir()     const override { return m_Root; }
        OpaaxString SaveDir()       const override { return m_Root; }
        OpaaxString TempDir()       const override { return m_Root; }

        OpaaxString EngineToAbsolute(const OpaaxString& InRel)  const override { return AssetToAbsolute(InRel); }
        OpaaxString ProjectToAbsolute(const OpaaxString& InRel) const override { return AssetToAbsolute(InRel); }

    private:
        OpaaxString m_Root;
    };

    InputModifierData Modifier(const EInputModifier InType)
    {
        InputModifierData lModifier;
        lModifier.Type = InType;
        return lModifier;
    }

    InputMappingEntry Mapping(const char* InActionPath, const EKeyCode InKey)
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

        FolderPaths      Paths;
        WorldManager     Worlds;
        ResourceManager  Resources;
        EngineEventBus   Events;
        InputManager     Input;
        EngineConfigData Config;

        // Last: the game ends before the services its context names.
        TStrongObjectPtr<GameInstance> Game;
    };
}

// =============================================================================
// The assets
// =============================================================================

TEST_CASE("InputActionAsset: an empty name binds as its file's name, and the file keeps every field")
{
    const TestTempDir  lDir("inputActionAsset");
    const InputClasses lClasses;

    InputActionAsset lSaved;
    lSaved.ValueType   = EInputValueType::Axis2D;
    lSaved.HoldSeconds = 1.25f;
    lSaved.Modifiers   = { Modifier(EInputModifier::DeadZone), Modifier(EInputModifier::Normalize) };
    REQUIRE(AssetFile::Save(lDir.Sub("Input/Move.opaaxasset"), lSaved));

    const TStrongObjectPtr<Asset> lLoaded = AssetFile::LoadAsset(lDir.Sub("Input/Move.opaaxasset"), lClasses);
    const InputActionAsset* lAction = Cast<InputActionAsset>(lLoaded.Get());
    REQUIRE(lAction != nullptr);

    // What the evaluator registers: the file's name, and the modifiers in their order.
    const InputAction lRuntime = lAction->ToAction();
    CHECK(lRuntime.Name == OPAAX_ID("Move"));
    CHECK(lRuntime.ValueType == EInputValueType::Axis2D);
    CHECK(lRuntime.HoldSeconds == doctest::Approx(1.25f));
    REQUIRE(lRuntime.Modifiers.size() == 2);
    CHECK(lRuntime.Modifiers[0].Type == EInputModifier::DeadZone);
    CHECK(lRuntime.Modifiers[1].Type == EInputModifier::Normalize);

    // A name of its own wins over the file's.
    InputActionAsset lNamed;
    lNamed.Name = OPAAX_ID("Walk");
    CHECK(lNamed.GetActionName() == OPAAX_ID("Walk"));
}

TEST_CASE("InputMappingContextAsset: a key is saved by its name, and each mapping keeps its modifiers")
{
    const TestTempDir  lDir("inputMapAsset");
    const InputClasses lClasses;

    InputMappingContextAsset lSaved;
    lSaved.Priority = 5;
    lSaved.Mappings = { Mapping("Input/Move.opaaxasset", EKeyCode::A) };
    lSaved.Mappings[0].Modifiers = { Modifier(EInputModifier::Negate) };
    lSaved.Mappings[0].bConsume  = false;

    const OpaaxString lPath = lDir.Sub("Input/Gameplay.opaaxasset");
    REQUIRE(AssetFile::Save(lPath, lSaved));

    // A person rebinding in a text editor reads "A", not an enum's number.
    AssetFile::Contents lContents;
    REQUIRE(AssetFile::Load(lPath, lContents));
    CHECK(lContents.Class == OPAAX_ID("InputMappingContextAsset"));
    CHECK(lContents.Properties["Mappings"][0]["Key"] == "A");

    const TStrongObjectPtr<Asset> lLoaded = AssetFile::LoadAsset(lPath, lClasses);
    const InputMappingContextAsset* lMap = Cast<InputMappingContextAsset>(lLoaded.Get());
    REQUIRE(lMap != nullptr);
    CHECK(lMap->Priority == 5);
    REQUIRE(lMap->EntryCount() == 1u);
    CHECK(lMap->Mappings[0].Action.Path == "Input/Move.opaaxasset");
    CHECK(lMap->Mappings[0].Key == EKeyCode::A);
    CHECK_FALSE(lMap->Mappings[0].bConsume);
    REQUIRE(lMap->Mappings[0].Modifiers.size() == 1);
    CHECK(lMap->Mappings[0].Modifiers[0].Type == EInputModifier::Negate);
}

TEST_CASE("InputAssets: the old .opaaxaction and .opaaxinputmap files convert with their values")
{
    const TestTempDir  lDir("inputConvert");
    const InputClasses lClasses;

    const TStrongObjectPtr<Asset> lAction = ConvertAndLoad(lDir, "Input/Charge.opaaxaction", R"({
        "Name": "Charge", "ValueType": "Bool", "HoldSeconds": 0.8,
        "Modifiers": [ { "Type": "Scalar", "Scale": { "x": 2.0, "y": 1.0 } } ], "Description": "Hold to charge" })",
        lClasses);

    REQUIRE(Cast<InputActionAsset>(lAction.Get()) != nullptr);
    const InputActionAsset& lCharge = *Cast<InputActionAsset>(lAction.Get());
    CHECK(lCharge.Name == OPAAX_ID("Charge"));
    CHECK(lCharge.HoldSeconds == doctest::Approx(0.8f));
    REQUIRE(lCharge.Modifiers.size() == 1);
    CHECK(lCharge.Modifiers[0].Scale.x == doctest::Approx(2.f));
    CHECK(lCharge.Description == "Hold to charge");

    const TStrongObjectPtr<Asset> lMapAsset = ConvertAndLoad(lDir, "Input/Controls.opaaxinputmap", R"({
        "Priority": 1,
        "Mappings": [ { "Action": "Input/Move.opaaxasset", "Key": "S",
                        "Modifiers": [ { "Type": "Swizzle" }, { "Type": "Negate" } ], "bConsume": true } ] })", lClasses);

    REQUIRE(Cast<InputMappingContextAsset>(lMapAsset.Get()) != nullptr);
    const InputMappingContextAsset& lControls = *Cast<InputMappingContextAsset>(lMapAsset.Get());
    CHECK(lControls.Priority == 1);
    REQUIRE(lControls.EntryCount() == 1u);
    CHECK(lControls.Mappings[0].Key == EKeyCode::S);
    REQUIRE(lControls.Mappings[0].Modifiers.size() == 2);
    CHECK(lControls.Mappings[0].Modifiers[1].Type == EInputModifier::Negate);
}

// =============================================================================
// AddContextAsset
// =============================================================================

TEST_CASE("AddContextAsset: the context's keys drive the actions its files describe")
{
    const TestTempDir  lDir("addContextAsset");
    const InputClasses lClasses;

    InputActionAsset lJump;   // named by its file
    REQUIRE(AssetFile::Save(lDir.Sub("Input/Jump.opaaxasset"), lJump));

    InputActionAsset lMove;
    lMove.Name      = OPAAX_ID("Walk");
    lMove.ValueType = EInputValueType::Axis2D;
    REQUIRE(AssetFile::Save(lDir.Sub("Input/Move.opaaxasset"), lMove));

    InputMappingContextAsset lMap;
    lMap.Mappings = {
        Mapping("Input/Jump.opaaxasset", EKeyCode::Space),
        Mapping("Input/Move.opaaxasset", EKeyCode::D),
        Mapping("Input/Missing.opaaxasset", EKeyCode::F),   // no such file: skipped
        Mapping("", EKeyCode::G),                           // no action: skipped
    };
    REQUIRE(AssetFile::Save(lDir.Sub("Input/Gameplay.opaaxasset"), lMap));

    ContextFixture        lFixture(lDir.Path(), lClasses);
    InputMappingSubsystem& lInput = lFixture.StartMapping();

    REQUIRE(lInput.AddContextAsset(OPAAX_ID("Gameplay"), OpaaxString("Input/Gameplay.opaaxasset")));
    CHECK(lInput.GetContextCount() == 1u);
    CHECK(lInput.GetActionCount() == 2u);

    lFixture.Press(lInput, EKeyCode::Space);
    CHECK(lInput.GetValue(OPAAX_ID("Jump")).AsBool());

    lFixture.Press(lInput, EKeyCode::D);
    CHECK(lInput.GetValue(OPAAX_ID("Walk")).AsAxis2D().x == doctest::Approx(1.f));

    // A context that does not load is refused, and changes nothing.
    CHECK_FALSE(lInput.AddContextAsset(OPAAX_ID("Nope"), OpaaxString("Input/Nope.opaaxasset")));
    CHECK(lInput.GetContextCount() == 1u);
}

TEST_CASE("AddContextAsset: a path written before the files were converted loads the converted one")
{
    const TestTempDir  lDir("addContextLegacyPath");
    const InputClasses lClasses;

    InputActionAsset lFire;
    REQUIRE(AssetFile::Save(lDir.Sub("Input/Fire.opaaxasset"), lFire));

    InputMappingContextAsset lMap;
    lMap.Mappings = { Mapping("Input/Fire.opaaxasset", EKeyCode::F) };
    REQUIRE(AssetFile::Save(lDir.Sub("Input/Test.opaaxasset"), lMap));

    ContextFixture        lFixture(lDir.Path(), lClasses);
    InputMappingSubsystem& lInput = lFixture.StartMapping();

    // Game code still naming the old file: --upgrade-assets rewrote the data, not this string.
    REQUIRE(lInput.AddContextAsset(OPAAX_ID("Test"), OpaaxString("Input/Test.opaaxinputmap")));

    lFixture.Press(lInput, EKeyCode::F);
    CHECK(lInput.GetValue(OPAAX_ID("Fire")).AsBool());

    // The old path names the same context for a rebind.
    const InputKeySlot lOldName{ OpaaxString("Input/Test.opaaxinputmap"), OPAAX_ID("Fire"), EKeyCode::F };
    REQUIRE(lInput.Rebind(lOldName, EKeyCode::G).IsOk());
    CHECK(lInput.GetKey(InputKeySlot{ OpaaxString("Input/Test.opaaxasset"), OPAAX_ID("Fire"), EKeyCode::F }) == EKeyCode::G);
}

// =============================================================================
// The player's keys
// =============================================================================

namespace
{
    // A walking context (Jump on Space, Use on E) and a locked menu (Pause on Escape), as files.
    void WriteControls(const TestTempDir& InDir)
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

    const OpaaxString CONTROLS_PATH("Input/Controls.opaaxasset");
    const OpaaxString MENU_PATH("Input/Menu.opaaxasset");

    InputKeySlot JumpSlot() { return InputKeySlot{ CONTROLS_PATH, OPAAX_ID("Jump"), EKeyCode::Space }; }
    InputKeySlot UseSlot()  { return InputKeySlot{ CONTROLS_PATH, OPAAX_ID("Use"), EKeyCode::E }; }

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

TEST_CASE("Rebind: a context known before it is added gets the player's key; a rebind applies at once")
{
    const TestTempDir  lDir("rebindApplies");
    const InputClasses lClasses;
    WriteControls(lDir);

    ContextFixture         lFixture(lDir.Path(), lClasses);
    InputMappingSubsystem& lInput = lFixture.StartMapping();

    Int32 lChanges = 0;
    lInput.OnKeysChanged().Add([&lChanges]() { ++lChanges; });

    // A controls menu, before the game adds the context.
    REQUIRE(lInput.RegisterContextAsset(CONTROLS_PATH));
    const TDynArray<InputKeyMapping> lMappings = lInput.GetKeyMappings(CONTROLS_PATH);
    REQUIRE(lMappings.size() == 2);
    CHECK(lMappings[0].Slot == JumpSlot());
    CHECK_FALSE(lMappings[0].IsRebound());

    REQUIRE(lInput.Rebind(JumpSlot(), EKeyCode::F).IsOk());
    CHECK(lChanges == 1);
    CHECK(lInput.GetKeyMappings(CONTROLS_PATH)[0].Key == EKeyCode::F);
    CHECK(lInput.GetContextCount() == 0u);

    REQUIRE(lInput.AddContextAsset(OPAAX_ID("Controls"), CONTROLS_PATH));
    CHECK(lFixture.Drives(lInput, EKeyCode::F, "Jump"));
    CHECK_FALSE(lFixture.Drives(lInput, EKeyCode::Space, "Jump"));

    // While added: at once.
    REQUIRE(lInput.Rebind(JumpSlot(), EKeyCode::G).IsOk());
    CHECK(lFixture.Drives(lInput, EKeyCode::G, "Jump"));
    CHECK_FALSE(lFixture.Drives(lInput, EKeyCode::F, "Jump"));

    // Swapped with Use: E jumps, G uses.
    const InputRebindResult lSwap = lInput.Rebind(JumpSlot(), EKeyCode::E, EKeyConflictPolicy::Swap);
    REQUIRE(lSwap.IsOk());
    REQUIRE(lSwap.Others.size() == 1);
    CHECK(lFixture.Drives(lInput, EKeyCode::E, "Jump"));
    CHECK(lFixture.Drives(lInput, EKeyCode::G, "Use"));

    // Cleared: no key drives it.
    REQUIRE(lInput.Rebind(JumpSlot(), EKeyCode::None).IsOk());
    CHECK_FALSE(lFixture.Drives(lInput, EKeyCode::E, "Jump"));

    lInput.ResetAllKeys();
    CHECK_FALSE(lInput.HasRebinds());
    CHECK(lFixture.Drives(lInput, EKeyCode::Space, "Jump"));
    CHECK(lFixture.Drives(lInput, EKeyCode::E, "Use"));
    CHECK(lChanges == 5);
}

TEST_CASE("Rebind: a menu's press-a-key, the key captured then bound; it jumps only once pressed again")
{
    const TestTempDir  lDir("rebindCaptured");
    const InputClasses lClasses;
    WriteControls(lDir);

    ContextFixture         lFixture(lDir.Path(), lClasses);
    InputMappingSubsystem& lInput = lFixture.StartMapping();
    REQUIRE(lInput.AddContextAsset(OPAAX_ID("Controls"), CONTROLS_PATH));

    ERebindResult lResult = ERebindResult::NoSuchMapping;
    lInput.OnKeyCaptured().Add([&lInput, &lResult](const EKeyCode InKey)
    {
        if (InKey != EKeyCode::None)
        {
            lResult = lInput.Rebind(JumpSlot(), InKey).Result;
        }
    });

    lInput.BeginKeyCapture();
    lFixture.Press(lInput, EKeyCode::G);
    CHECK(lResult == ERebindResult::Rebound);
    CHECK(lInput.GetKey(JumpSlot()) == EKeyCode::G);

    // The key that was taken does not jump on its own press, nor while held.
    CHECK_FALSE(lInput.GetValue(OPAAX_ID("Jump")).AsBool());
    lInput.Update(1.0 / 60.0);
    lFixture.Input.EndFrame();
    CHECK_FALSE(lInput.GetValue(OPAAX_ID("Jump")).AsBool());

    lFixture.Release(lInput, EKeyCode::G);
    CHECK(lFixture.Drives(lInput, EKeyCode::G, "Jump"));

    // A key another mapping has: the menu learns it, and asks.
    lInput.BeginKeyCapture();
    lFixture.Press(lInput, EKeyCode::E);
    CHECK(lResult == ERebindResult::Conflict);
    CHECK(lInput.GetKey(JumpSlot()) == EKeyCode::G);
}

TEST_CASE("Rebind: a locked context's keys are reserved once it is known, and are not the player's")
{
    const TestTempDir  lDir("rebindLocked");
    const InputClasses lClasses;
    WriteControls(lDir);

    ContextFixture         lFixture(lDir.Path(), lClasses);
    InputMappingSubsystem& lInput = lFixture.StartMapping();
    REQUIRE(lInput.AddContextAsset(OPAAX_ID("Controls"), CONTROLS_PATH));

    // Not known yet: nothing reserves Escape.
    CHECK(lInput.Rebind(JumpSlot(), EKeyCode::Escape).IsOk());
    lInput.ResetKey(JumpSlot());

    REQUIRE(lInput.RegisterContextAsset(MENU_PATH));
    const InputRebindResult lResult = lInput.Rebind(JumpSlot(), EKeyCode::Escape, EKeyConflictPolicy::Replace);
    CHECK(lResult.Result == ERebindResult::Reserved);
    REQUIRE(lResult.Others.size() == 1);
    CHECK(lResult.Others[0].Slot.Action == OPAAX_ID("Pause"));

    CHECK(lInput.Rebind(InputKeySlot{ MENU_PATH, OPAAX_ID("Pause"), EKeyCode::Escape }, EKeyCode::P).Result
          == ERebindResult::Locked);

    // A mapping no known context has.
    CHECK(lInput.Rebind(InputKeySlot{ CONTROLS_PATH, OPAAX_ID("Jump"), EKeyCode::J }, EKeyCode::K).Result
          == ERebindResult::NoSuchMapping);
}

TEST_CASE("Rebind: the game's key check refuses a key, and says why")
{
    const TestTempDir  lDir("rebindCheck");
    const InputClasses lClasses;
    WriteControls(lDir);

    ContextFixture         lFixture(lDir.Path(), lClasses);
    InputMappingSubsystem& lInput = lFixture.StartMapping();
    REQUIRE(lInput.AddContextAsset(OPAAX_ID("Controls"), CONTROLS_PATH));

    lInput.SetKeyCheck([](const InputKeySlot&, const EKeyCode InKey, OpaaxString& OutWhy)
    {
        OutWhy = OpaaxString("the wheel is the zoom");
        return InKey != EKeyCode::Mouse_WheelUp && InKey != EKeyCode::Mouse_WheelDown;
    });

    const InputRebindResult lResult = lInput.Rebind(JumpSlot(), EKeyCode::Mouse_WheelUp);
    CHECK(lResult.Result == ERebindResult::RefusedByGame);
    CHECK(lResult.Why == "the wheel is the zoom");
    CHECK(lInput.GetKey(JumpSlot()) == EKeyCode::Space);

    CHECK(lInput.Rebind(JumpSlot(), EKeyCode::Mouse_Left).IsOk());

    lInput.SetKeyCheck({});
    CHECK(lInput.Rebind(JumpSlot(), EKeyCode::Mouse_WheelUp).IsOk());
}

TEST_CASE("Rebind: the keys are kept in the player's settings, read at the next launch and when they change there")
{
    const TestTempDir  lDir("rebindSettings");
    const InputClasses lClasses;
    WriteControls(lDir);

    ContextFixture lFixture(lDir.Path(), lClasses);
    PlayerSettings lPlayer(lDir, lFixture.Paths);

    TUniquePtr<UserSettings> lSettings = lPlayer.Launch();
    lFixture.Settings                  = lSettings.get();
    {
        InputMappingSubsystem& lInput = lFixture.StartMapping();

        // Not known yet: no such mapping, nothing kept.
        CHECK(lInput.Rebind(UseSlot(), EKeyCode::F).Result == ERebindResult::NoSuchMapping);
        CHECK(lSettings->GetSection(InputMappingSubsystem::KEYS_SECTION).is_null());

        REQUIRE(lInput.AddContextAsset(OPAAX_ID("Controls"), CONTROLS_PATH));
        REQUIRE(lInput.Rebind(JumpSlot(), EKeyCode::Q).IsOk());
        CHECK(lSettings->GetSection(InputMappingSubsystem::KEYS_SECTION)
              == nlohmann::json::parse(R"({"Input/Controls.opaaxasset": {"Jump": {"Space": "Q"}}})"));
        CHECK(lSettings->HasUnsavedChanges());
        REQUIRE(lSettings->Save() == ESaveResult::Ok);
        lFixture.Game->End();
    }

    // The next launch: the file read, the context added with the player's key.
    lSettings         = lPlayer.Launch();
    lFixture.Settings = lSettings.get();
    {
        InputMappingSubsystem& lInput = lFixture.StartMapping();
        CHECK(lInput.GetKey(JumpSlot()) == EKeyCode::Q);
        REQUIRE(lInput.AddContextAsset(OPAAX_ID("Controls"), CONTROLS_PATH));
        CHECK(lFixture.Drives(lInput, EKeyCode::Q, "Jump"));

        // Changed in the settings, by a menu's Reset or an edit: read again at once.
        Int32 lChanges = 0;
        lInput.OnKeysChanged().Add([&lChanges]() { ++lChanges; });

        lSettings->SetSection(InputMappingSubsystem::KEYS_SECTION,
                              nlohmann::json::parse(R"({"Input/Controls.opaaxasset": {"Jump": {"Space": "Z"}}})"));
        CHECK(lInput.GetKey(JumpSlot()) == EKeyCode::Z);
        CHECK(lFixture.Drives(lInput, EKeyCode::Z, "Jump"));

        lSettings->Reset();
        CHECK(lInput.GetKey(JumpSlot()) == EKeyCode::Space);
        CHECK(lFixture.Drives(lInput, EKeyCode::Space, "Jump"));
        CHECK(lChanges == 2);

        // Read from the file again: Q.
        lSettings->Load();
        CHECK(lInput.GetKey(JumpSlot()) == EKeyCode::Q);
        lFixture.Game->End();
    }
}

TEST_CASE("Rebind: keys kept for a mapping the asset no longer has, or for a locked context, are dropped once it is read")
{
    const TestTempDir  lDir("rebindDropped");
    const InputClasses lClasses;
    WriteControls(lDir);

    ContextFixture lFixture(lDir.Path(), lClasses);
    PlayerSettings lPlayer(lDir, lFixture.Paths);

    TUniquePtr<UserSettings> lSettings = lPlayer.Launch();
    lSettings->SetSection(InputMappingSubsystem::KEYS_SECTION, nlohmann::json::parse(R"({
        "Input/Controls.opaaxasset": {"Jump": {"Space": "Q"}, "Dash": {"LeftShift": "X"}},
        "Input/Menu.opaaxasset": {"Pause": {"Escape": "P"}}})"));
    lFixture.Settings = lSettings.get();

    InputMappingSubsystem& lInput = lFixture.StartMapping();
    REQUIRE(lInput.AddContextAsset(OPAAX_ID("Controls"), CONTROLS_PATH));
    REQUIRE(lInput.RegisterContextAsset(MENU_PATH));

    CHECK(lInput.GetKey(JumpSlot()) == EKeyCode::Q);
    CHECK(lSettings->GetSection(InputMappingSubsystem::KEYS_SECTION)
          == nlohmann::json::parse(R"({"Input/Controls.opaaxasset": {"Jump": {"Space": "Q"}}})"));
    lFixture.Game->End();
}
