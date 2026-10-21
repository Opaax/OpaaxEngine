// Suite: the input assets — InputActionAsset and InputMappingContextAsset — through their files, the
// conversion of the old .opaaxaction and .opaaxinputmap formats, and AddContextAsset, which turns them
// into the evaluator's actions and bindings. File cases use a temp directory of their own.
#include <doctest.h>

#include "../LegacyAssetFile.h"
#include "../TestTempDir.h"

#include "Application/Services/IPaths.h"
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

        GameInstanceContext Make() { return GameInstanceContext{Worlds, Resources, Paths, Events, Input, Config}; }

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
}
