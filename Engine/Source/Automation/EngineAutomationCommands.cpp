#include "Automation/EngineAutomationCommands.h"

#include "Application/OpaaxApplication.h"
#include "Application/Services/IEngine.h"
#include "Application/Services/IPaths.h"
#include "Core/Algo/Algo.h"
#include "Core/IO/FileSystem.h"
#include "Core/Maths/Maths.h"
#include "Core/String/OpaaxStringJson.h"
#include "Core/Time/Clock.h"
#include "Engine/GameInstance/GameInstance.h"
#include "Engine/GameInstance/GameInstanceManager.h"
#include "Engine/Registries/EngineRegistries.h"
#include "Input/InputKeyNames.h"
#include "Input/InputManager.h"
#include "UI/UICanvas.h"
#include "UI/UISubsystem.h"
#include "UI/UIWidget.h"
#include "World/Behaviour/BehaviourSubsystem.h"
#include "World/Components/ComponentRegistry.h"
#include "World/Entity/Entity.h"
#include "World/Entity/EntityMeta.h"
#include "World/World.h"
#include "World/WorldManager.h"
#include "World/WorldSpec.h"

namespace Opaax::EngineAutomation
{
    namespace
    {
        /** The frames a tapped key stays down, so a fixed-step game surely sees it. */
        constexpr Uint32 DEFAULT_TAP_FRAMES = 2;

        /** The key or mouse button InName names ("Space", "A", "Mouse_Left"...). */
        bool ParseKey(const OpaaxString& InName, EKeyCode& OutKey)
        {
            for (const EKeyCode lKey : TEnumValues<EKeyCode>::Values)
            {
                if (InName == ToString(lKey) && IsKeyCodeBindable(lKey))
                {
                    OutKey = lKey;
                    return true;
                }
            }
            return false;
        }

        bool IsMouseButton(const EKeyCode InKey)
        {
            return OpaaxStringView(ToString(InKey)).StartsWith("Mouse_");
        }

        void Press(InputManager& InInput, const EKeyCode InKey)
        {
            if (IsMouseButton(InKey)) { InInput.OnMouseButtonPressed(InKey); }
            else                      { InInput.OnKeyPressed(InKey, /*InRepeat*/ false); }
        }

        void Release(InputManager& InInput, const EKeyCode InKey)
        {
            if (IsMouseButton(InKey)) { InInput.OnMouseButtonReleased(InKey); }
            else                      { InInput.OnKeyReleased(InKey); }
        }

        /** The running game's UI canvas, or null with OutError. */
        UICanvas* GameCanvas(IEngine& InEngine, OpaaxString& OutError)
        {
            GameInstance* const lGame = InEngine.GetGameInstances().GetGameInstance();
            UISubsystem* const  lUI   = (lGame != nullptr) ? lGame->GetSubsystems().GetSubsystem<UISubsystem>() : nullptr;
            if (lUI == nullptr)
            {
                OutError = "no game is running: the UI belongs to the game (play a level first)";
                return nullptr;
            }
            return &lUI->GetCanvas();
        }

        /** Visible with every parent visible. */
        bool IsShown(const UIWidget& InWidget)
        {
            for (const UIWidget* lNode = &InWidget; lNode != nullptr; lNode = lNode->GetParent())
            {
                if (!lNode->bVisible)
                {
                    return false;
                }
            }
            return true;
        }

        void DescribeTree(const UICanvas& InCanvas, const UIWidget& InWidget, const OpaaxString& InName,
                          nlohmann::json& OutList)
        {
            if (InName.IsEmpty() || InName == InWidget.Name)
            {
                const Bounds2D& lBounds   = InWidget.GetBounds();
                const Vector2F  lCentre   = InCanvas.CanvasToScreen(lBounds.Center);
                const float     lPerPixel = InCanvas.UnitsPerPixel();

                OutList.push_back(nlohmann::json{
                    { "name",    InWidget.Name },
                    { "type",    InWidget.GetTypeName().CStr() },
                    { "visible", IsShown(InWidget) },
                    { "x",       lCentre.x },
                    { "y",       lCentre.y },
                    { "width",   lBounds.HalfExtent.x * 2.f / lPerPixel },
                    { "height",  lBounds.HalfExtent.y * 2.f / lPerPixel } });
            }

            for (const TUniquePtr<UIWidget>& lChild : InWidget.GetChildren())
            {
                DescribeTree(InCanvas, *lChild, InName, OutList);
            }
        }

        World* ActiveWorld(IEngine& InEngine, OpaaxString& OutError)
        {
            World* lWorld = InEngine.GetWorldManager().GetActiveWorld();
            if (lWorld == nullptr)
            {
                OutError = "no active world";
            }
            return lWorld;
        }

        /** How far apart two numbers can be and still be "equal" (floats written back and forth). */
        constexpr double EQUALS_TOLERANCE = 1.0e-4;

        /**
         * Whether InActual matches InExpected: numbers within InTolerance, an object on the fields
         * InExpected gives, arrays element by element, anything else exactly.
         */
        bool Matches(const nlohmann::json& InActual, const nlohmann::json& InExpected, const double InTolerance)
        {
            if (InActual.is_number() && InExpected.is_number())
            {
                return Maths::Abs(InActual.get<double>() - InExpected.get<double>()) <= InTolerance;
            }

            if (InActual.is_object() && InExpected.is_object())
            {
                for (const auto& [lKey, lValue] : InExpected.items())
                {
                    const auto lField = InActual.find(lKey);
                    if (lField == InActual.end() || !Matches(*lField, lValue, InTolerance))
                    {
                        return false;
                    }
                }
                return true;
            }

            if (InActual.is_array() && InExpected.is_array())
            {
                if (InActual.size() != InExpected.size())
                {
                    return false;
                }
                for (Uint64 lIndex = 0; lIndex < InActual.size(); ++lIndex)
                {
                    if (!Matches(InActual[lIndex], InExpected[lIndex], InTolerance))
                    {
                        return false;
                    }
                }
                return true;
            }

            return InActual == InExpected;
        }

        /** The entities of InWorld whose name is InName (any when empty) and that have InComponent (any when empty). */
        Uint64 CountEntities(World& InWorld, const ComponentRegistry& InTypes, const OpaaxString& InName,
                             const OpaaxString& InComponent)
        {
            const IComponentEntry* lEntry = InComponent.IsEmpty() ? nullptr : InTypes.FindByName(OpaaxStringID(InComponent));
            if (!InComponent.IsEmpty() && lEntry == nullptr)
            {
                return 0;
            }

            Uint64 lCount = 0;
            InWorld.Each<EntityMeta>([&](const EntityID InId, const EntityMeta& InMeta)
            {
                if (!InName.IsEmpty() && InName != InMeta.Name)
                {
                    return;
                }
                if (lEntry != nullptr && !lEntry->Has(InWorld.GetRegistry(), InId))
                {
                    return;
                }
                ++lCount;
            });
            return lCount;
        }

        // =========================================================================
        // App
        // =========================================================================
        void RegisterApp(AutomationRunner& InRunner, IAutomationHost& InHost)
        {
            InRunner.Register("app.info", "The app's frame and folders: {frame, workspace, project, assets, save}.",
                [&InHost](const nlohmann::json&)
                {
                    const IPaths& lPaths = OpaaxApplication::GetAppService<IPaths>();
                    return AutomationResult::Ok(nlohmann::json{
                        { "frame",     InHost.GetFrameIndex() },
                        { "workspace", lPaths.WorkspaceRoot() },
                        { "project",   lPaths.ProjectRoot() },
                        { "assets",    lPaths.AssetsDir() },
                        { "save",      lPaths.SaveDir() } });
                });

            InRunner.Register("app.quit", "Closes the app at the end of this frame.",
                [&InHost](const nlohmann::json&)
                {
                    InHost.RequestQuit();
                    return AutomationResult::Ok();
                });

            InRunner.Register("expect.quit",
                "Holds the queue until the game closes the app itself (QuitGame), then answers ok; fails if the "
                "app is still open after {seconds} (default 10, real time).",
                [](const nlohmann::json& InParams)
                {
                    const double lSeconds  = InParams.value("seconds", 10.0);
                    const double lDeadline = Clock::NowSeconds() + lSeconds;

                    AutomationResult lResult;
                    lResult.WaitUntil = [lDeadline]() { return Clock::NowSeconds() >= lDeadline; };
                    lResult.Answer    = [lSeconds]()
                    {
                        return AutomationResult::Fail("the app is still open after " + ToJsonText(nlohmann::json(lSeconds)) + " s");
                    };
                    lResult.OnClose   = []() { return AutomationResult::Ok(nlohmann::json{ { "closed", true } }); };
                    return lResult;
                });

            InRunner.Register("screenshot",
                "Saves this frame as a PNG at {path} (relative paths are from the working folder); the next "
                "request runs once it is written.",
                [&InHost](const nlohmann::json& InParams)
                {
                    const OpaaxString lPath = InParams.value("path", OpaaxString());
                    if (lPath.IsEmpty())
                    {
                        return AutomationResult::Fail("\"path\" names the PNG to write");
                    }

                    const OpaaxString lAbsolute = FileSystem::MakeAbsolute(lPath);
                    InHost.RequestScreenshot(lAbsolute);

                    AutomationResult lResult = AutomationResult::Ok(nlohmann::json{ { "path", lAbsolute } });
                    lResult.WaitFrames = 1;
                    return lResult;
                });
        }

        // =========================================================================
        // Input
        // =========================================================================
        void RegisterInput(AutomationRunner& InRunner, IEngine& InEngine)
        {
            InRunner.Register("input.key",
                "Feeds {key} (\"Space\", \"A\", \"Mouse_Left\"...) to the game: {action} \"press\", \"release\" or "
                "\"tap\" (down for {frames}, default 2, then up).",
                [&InEngine](const nlohmann::json& InParams)
                {
                    EKeyCode          lKey = EKeyCode::None;
                    const OpaaxString lName = InParams.value("key", OpaaxString());
                    if (!ParseKey(lName, lKey))
                    {
                        return AutomationResult::Fail("unknown key '" + lName + "'");
                    }

                    InputManager&     lInput  = InEngine.GetInput();
                    const OpaaxString lAction = InParams.value("action", OpaaxString("tap"));
                    if (lAction == "press")
                    {
                        Press(lInput, lKey);
                        return AutomationResult::Ok();
                    }
                    if (lAction == "release")
                    {
                        Release(lInput, lKey);
                        return AutomationResult::Ok();
                    }
                    if (lAction != "tap")
                    {
                        return AutomationResult::Fail("\"action\" is press, release or tap");
                    }

                    Press(lInput, lKey);
                    AutomationResult lResult;
                    lResult.WaitFrames = Maths::Max(InParams.value("frames", DEFAULT_TAP_FRAMES), 1u);
                    lResult.AfterWait  = [&lInput, lKey] { Release(lInput, lKey); };
                    return lResult;
                });

            InRunner.Register("input.mouse",
                "Moves the game's pointer to {x, y} (pixels of the game view, from its top left).",
                [&InEngine](const nlohmann::json& InParams)
                {
                    InEngine.GetInput().OnMouseMoved(InParams.at("x").get<float>(), InParams.at("y").get<float>());
                    return AutomationResult::Ok();
                });
        }

        // =========================================================================
        // The game's UI
        // =========================================================================
        void RegisterUI(AutomationRunner& InRunner, IEngine& InEngine)
        {
            InRunner.Register("ui.list",
                "The game UI's widgets, depth-first: [{name, type, visible, x, y, width, height}], x and y their "
                "centre in game-view pixels. Filter: {name}.",
                [&InEngine](const nlohmann::json& InParams)
                {
                    OpaaxString     lError;
                    UICanvas* const lCanvas = GameCanvas(InEngine, lError);
                    if (lCanvas == nullptr)
                    {
                        return AutomationResult::Fail(lError);
                    }
                    return AutomationResult::Ok(nlohmann::json{
                        { "widgets", DescribeWidgets(*lCanvas, InParams.value("name", OpaaxString())) } });
                });

            InRunner.Register("ui.click",
                "Clicks the game UI's widget named {widget}: the pointer moves to its centre and the left button "
                "is tapped (down for {frames}, default 2).",
                [&InEngine](const nlohmann::json& InParams)
                {
                    OpaaxString     lError;
                    UICanvas* const lCanvas = GameCanvas(InEngine, lError);
                    Vector2F        lPixel  = { 0.f, 0.f };
                    if (lCanvas == nullptr
                        || !FindWidgetCentre(*lCanvas, InParams.value("widget", OpaaxString()), lPixel, lError))
                    {
                        return AutomationResult::Fail(lError);
                    }

                    // Moved and pressed in one frame: the UI routes the move first, so the press lands on it.
                    InputManager& lInput = InEngine.GetInput();
                    lInput.OnMouseMoved(lPixel.x, lPixel.y);
                    Press(lInput, EKeyCode::Mouse_Left);

                    AutomationResult lResult = AutomationResult::Ok(nlohmann::json{ { "x", lPixel.x }, { "y", lPixel.y } });
                    lResult.WaitFrames = Maths::Max(InParams.value("frames", DEFAULT_TAP_FRAMES), 1u);
                    lResult.AfterWait  = [&lInput] { Release(lInput, EKeyCode::Mouse_Left); };
                    return lResult;
                });
        }

        // =========================================================================
        // World
        // =========================================================================
        void RegisterWorld(AutomationRunner& InRunner, IEngine& InEngine)
        {
            InRunner.Register("world.info", "The active world: {name, mode, paused, entities, camera}.",
                [&InEngine](const nlohmann::json&)
                {
                    OpaaxString  lError;
                    World* const lWorld = ActiveWorld(InEngine, lError);
                    if (lWorld == nullptr)
                    {
                        return AutomationResult::Fail(lError);
                    }

                    const CameraView& lView = lWorld->GetCameraView();
                    return AutomationResult::Ok(nlohmann::json{
                        { "name",     lWorld->GetName() },
                        { "mode",     ToString(lWorld->GetMode()) },
                        { "paused",   InEngine.GetWorldManager().IsPaused() },
                        { "entities", lWorld->GetEntityCount() },
                        { "camera",   { { "x", lView.Position.x }, { "y", lView.Position.y }, { "size", lView.OrthoSize } } } });
                });

            InRunner.Register("entity.list",
                "The active world's entities: [{id, name, parent, map, components}]. Filters: {name}, {component}.",
                [&InEngine](const nlohmann::json& InParams)
                {
                    OpaaxString  lError;
                    World* const lWorld = ActiveWorld(InEngine, lError);
                    if (lWorld == nullptr)
                    {
                        return AutomationResult::Fail(lError);
                    }

                    const ComponentRegistry& lTypes     = InEngine.GetRegistries().Components();
                    const OpaaxString        lName      = InParams.value("name", OpaaxString());
                    const OpaaxString        lComponent = InParams.value("component", OpaaxString());

                    nlohmann::json lList = nlohmann::json::array();
                    lWorld->Each<EntityMeta>([&](const EntityID InId, const EntityMeta& InMeta)
                    {
                        if (!lName.IsEmpty() && lName != InMeta.Name)
                        {
                            return;
                        }

                        nlohmann::json lEntity = DescribeEntity(Entity{ InId, lWorld }, lTypes, false);
                        if (!lComponent.IsEmpty() && !Algo::Contains(lEntity["components"], nlohmann::json(lComponent)))
                        {
                            return;
                        }
                        lList.push_back(Move(lEntity));
                    });

                    return AutomationResult::Ok(nlohmann::json{ { "entities", lList } });
                });

            InRunner.Register("entity.get", "One entity of the active world with its components' values: {entity} (id or name).",
                [&InEngine](const nlohmann::json& InParams)
                {
                    OpaaxString  lError;
                    World* const lWorld = ActiveWorld(InEngine, lError);
                    if (lWorld == nullptr)
                    {
                        return AutomationResult::Fail(lError);
                    }

                    const Entity lEntity = FindEntity(*lWorld, InParams, lError);
                    if (!lEntity.IsValid())
                    {
                        return AutomationResult::Fail(lError);
                    }

                    return AutomationResult::Ok(DescribeEntity(lEntity, InEngine.GetRegistries().Components(), true));
                });

            InRunner.Register("component.set",
                "Changes fields of {entity}'s component {type}: {value} is merged into it (added when missing).",
                [&InEngine](const nlohmann::json& InParams)
                {
                    OpaaxString  lError;
                    World* const lWorld = ActiveWorld(InEngine, lError);
                    if (lWorld == nullptr)
                    {
                        return AutomationResult::Fail(lError);
                    }

                    const Entity lEntity = FindEntity(*lWorld, InParams, lError);
                    if (!lEntity.IsValid())
                    {
                        return AutomationResult::Fail(lError);
                    }

                    const ComponentRegistry& lTypes = InEngine.GetRegistries().Components();
                    if (!PatchComponent(lEntity, lTypes, InParams, lError))
                    {
                        return AutomationResult::Fail(lError);
                    }

                    lWorld->MarkChanged();
                    return AutomationResult::Ok(DescribeEntity(lEntity, lTypes, true));
                });
        }

        // =========================================================================
        // Levels
        // =========================================================================
        void RegisterLevels(AutomationRunner& InRunner, IEngine& InEngine)
        {
            InRunner.Register("level.play",
                "Opens the level at {path} (relative to the project's assets) in a new play world; the next request "
                "runs once it is open.",
                [&InEngine](const nlohmann::json& InParams)
                {
                    const OpaaxString lPath     = InParams.value("path", OpaaxString());
                    const OpaaxString lAbsolute = OpaaxApplication::GetAppService<IPaths>().AssetToAbsolute(lPath);
                    if (lPath.IsEmpty() || !FileSystem::IsFile(lAbsolute))
                    {
                        return AutomationResult::Fail("no level at '" + lPath + "' (paths are relative to the assets)");
                    }

                    WorldSpec lSpec;
                    lSpec.LevelPath = lPath;
                    lSpec.Mode      = EWorldMode::Play;
                    InEngine.RequestOpenLevel(lSpec);

                    // Opened at the start of the next frame, started during it.
                    AutomationResult lResult = AutomationResult::Ok(nlohmann::json{ { "path", lPath } });
                    lResult.WaitFrames = 2;
                    return lResult;
                });

            InRunner.Register("world.wait",
                "Lets the playing world's clock advance {seconds} (game time, whatever the frame rate) before the "
                "next request. Ends early if the world changes.",
                [&InEngine](const nlohmann::json& InParams)
                {
                    OpaaxString  lError;
                    World* const lWorld = ActiveWorld(InEngine, lError);
                    if (lWorld == nullptr)
                    {
                        return AutomationResult::Fail(lError);
                    }

                    const BehaviourSubsystem* lRuntime = lWorld->GetSubsystems().GetSubsystem<BehaviourSubsystem>();
                    if (lRuntime == nullptr)
                    {
                        return AutomationResult::Fail("the active world is not playing: it has no game clock");
                    }

                    const double lUntil = lRuntime->GetTime() + InParams.value("seconds", 1.0);

                    AutomationResult lResult;
                    lResult.WaitUntil = [&InEngine, lWorld, lUntil]()
                    {
                        World* const lActive = InEngine.GetWorldManager().GetActiveWorld();
                        if (lActive != lWorld)
                        {
                            return true;
                        }
                        const BehaviourSubsystem* lClock = lActive->GetSubsystems().GetSubsystem<BehaviourSubsystem>();
                        return lClock == nullptr || lClock->GetTime() >= lUntil;
                    };
                    return lResult;
                });
        }

        // =========================================================================
        // Expectations: a failed one fails the request, so a script's exit code fails a test run
        // =========================================================================
        void RegisterExpectations(AutomationRunner& InRunner, IEngine& InEngine)
        {
            InRunner.Register("expect.value",
                "Fails unless {entity}'s value at {path} (\"Transform/Position/x\") meets: equals, near (with "
                "tolerance), greater, less or between [min, max].",
                [&InEngine](const nlohmann::json& InParams)
                {
                    OpaaxString  lError;
                    World* const lWorld = ActiveWorld(InEngine, lError);
                    if (lWorld == nullptr)
                    {
                        return AutomationResult::Fail(lError);
                    }

                    const Entity lEntity = FindEntity(*lWorld, InParams, lError);
                    if (!lEntity.IsValid())
                    {
                        return AutomationResult::Fail(lError);
                    }

                    const OpaaxString    lPath    = InParams.value("path", OpaaxString());
                    const nlohmann::json lValues  = DescribeEntity(lEntity, InEngine.GetRegistries().Components(), true);
                    const auto           lPointer = nlohmann::json::json_pointer(ToJsonString("/components/" + lPath));
                    if (lPath.IsEmpty() || !lValues.contains(lPointer))
                    {
                        return AutomationResult::Fail("no value at '" + lPath + "' (Component/Field/...)");
                    }

                    const nlohmann::json& lActual = lValues.at(lPointer);
                    if (!CheckExpectation(lActual, InParams, lError))
                    {
                        return AutomationResult::Fail(InParams.at("entity").get<OpaaxString>() + " " + lPath + ": " + lError);
                    }
                    return AutomationResult::Ok(nlohmann::json{ { "value", lActual } });
                });

            InRunner.Register("expect.count",
                "Fails unless the number of entities ({name}, {component} filter them) meets: equals, greater, "
                "less or between [min, max].",
                [&InEngine](const nlohmann::json& InParams)
                {
                    OpaaxString  lError;
                    World* const lWorld = ActiveWorld(InEngine, lError);
                    if (lWorld == nullptr)
                    {
                        return AutomationResult::Fail(lError);
                    }

                    const Uint64 lCount = CountEntities(*lWorld, InEngine.GetRegistries().Components(),
                                                        InParams.value("name", OpaaxString()),
                                                        InParams.value("component", OpaaxString()));
                    if (!CheckExpectation(nlohmann::json(lCount), InParams, lError))
                    {
                        return AutomationResult::Fail("entity count: " + lError);
                    }
                    return AutomationResult::Ok(nlohmann::json{ { "count", lCount } });
                });

            InRunner.Register("expect.entity",
                "Fails unless an entity named {entity} (or with that id) exists; with {exists: false}, unless none does.",
                [&InEngine](const nlohmann::json& InParams)
                {
                    OpaaxString  lError;
                    World* const lWorld = ActiveWorld(InEngine, lError);
                    if (lWorld == nullptr)
                    {
                        return AutomationResult::Fail(lError);
                    }

                    const OpaaxString lRef = InParams.value("entity", OpaaxString());
                    if (lRef.IsEmpty())
                    {
                        return AutomationResult::Fail("\"entity\" names an entity: its id or its name");
                    }

                    // By name, any number of them counts.
                    const bool bWanted = InParams.value("exists", true);
                    Guid       lId;
                    const bool bFound  = Guid::FromString(lRef, lId)
                                             ? lWorld->FindByGuid(lId).IsValid()
                                             : CountEntities(*lWorld, InEngine.GetRegistries().Components(), lRef, {}) > 0;
                    if (bFound != bWanted)
                    {
                        return AutomationResult::Fail("'" + lRef + (bFound ? "' exists" : "' does not exist"));
                    }
                    return AutomationResult::Ok(nlohmann::json{ { "exists", bFound } });
                });
        }
    }

    void Register(AutomationRunner& InRunner, IEngine& InEngine, IAutomationHost& InHost)
    {
        RegisterApp(InRunner, InHost);
        RegisterInput(InRunner, InEngine);
        RegisterUI(InRunner, InEngine);
        RegisterWorld(InRunner, InEngine);
        RegisterLevels(InRunner, InEngine);
        RegisterExpectations(InRunner, InEngine);
    }

    nlohmann::json DescribeWidgets(const UICanvas& InCanvas, const OpaaxString& InName)
    {
        nlohmann::json lList = nlohmann::json::array();
        DescribeTree(InCanvas, InCanvas.Root(), InName, lList);
        return lList;
    }

    bool FindWidgetCentre(UICanvas& InCanvas, const OpaaxString& InName, Vector2F& OutPixel, OpaaxString& OutError)
    {
        UIWidget* const lWidget = InName.IsEmpty() ? nullptr : InCanvas.Root().FindByName(InName);
        if (lWidget == nullptr)
        {
            OutError = "no widget named '" + InName + "' in the game's UI (ui.list lists them)";
            return false;
        }
        if (!IsShown(*lWidget))
        {
            OutError = "'" + InName + "' is hidden";
            return false;
        }

        OutPixel = InCanvas.CanvasToScreen(lWidget->GetBounds().Center);
        return true;
    }

    bool CheckExpectation(const nlohmann::json& InActual, const nlohmann::json& InParams, OpaaxString& OutError)
    {
        const auto lFail = [&InActual, &OutError](const OpaaxString& InExpected)
        {
            OutError = "expected " + InExpected + ", found " + ToJsonText(InActual);
            return false;
        };

        if (const auto lEquals = InParams.find("equals"); lEquals != InParams.end()
            && !Matches(InActual, *lEquals, EQUALS_TOLERANCE))
        {
            return lFail(ToJsonText(*lEquals));
        }

        if (const auto lNear = InParams.find("near"); lNear != InParams.end())
        {
            const double lTolerance = InParams.value("tolerance", 0.01);
            if (!Matches(InActual, *lNear, lTolerance))
            {
                return lFail(ToJsonText(*lNear) + " within " + ToJsonText(nlohmann::json(lTolerance)));
            }
        }

        // The comparisons need a number.
        const bool bCompares = InParams.contains("greater") || InParams.contains("less") || InParams.contains("between");
        if (bCompares && !InActual.is_number())
        {
            return lFail("a number");
        }

        if (const auto lGreater = InParams.find("greater"); lGreater != InParams.end()
            && !(InActual.get<double>() > lGreater->get<double>()))
        {
            return lFail("more than " + ToJsonText(*lGreater));
        }

        if (const auto lLess = InParams.find("less"); lLess != InParams.end()
            && !(InActual.get<double>() < lLess->get<double>()))
        {
            return lFail("less than " + ToJsonText(*lLess));
        }

        if (const auto lBetween = InParams.find("between"); lBetween != InParams.end())
        {
            const double lValue = InActual.get<double>();
            if (!lBetween->is_array() || lBetween->size() != 2)
            {
                OutError = "\"between\" is [min, max]";
                return false;
            }
            if (lValue < (*lBetween)[0].get<double>() || lValue > (*lBetween)[1].get<double>())
            {
                return lFail("between " + ToJsonText((*lBetween)[0]) + " and " + ToJsonText((*lBetween)[1]));
            }
        }

        return true;
    }

    Entity FindEntity(World& InWorld, const nlohmann::json& InParams, OpaaxString& OutError)
    {
        const auto lRef = InParams.find("entity");
        if (lRef == InParams.end() || !lRef->is_string() || lRef->get<OpaaxString>().IsEmpty())
        {
            OutError = "\"entity\" names an entity: its id or its name";
            return Entity{};
        }

        const OpaaxString lText = lRef->get<OpaaxString>();

        Guid lId;
        if (Guid::FromString(lText, lId))
        {
            const Entity lEntity = InWorld.FindByGuid(lId);
            if (!lEntity.IsValid())
            {
                OutError = "no entity with the id " + lText;
            }
            return lEntity;
        }

        // By name: only when one entity has it.
        Entity lFound;
        Uint32 lMatches = 0;
        InWorld.Each<EntityMeta>([&](const EntityID InId, const EntityMeta& InMeta)
        {
            if (lText == InMeta.Name)
            {
                lFound = Entity{ InId, &InWorld };
                ++lMatches;
            }
        });

        if (lMatches == 0)
        {
            OutError = "no entity named '" + lText + "'";
            return Entity{};
        }
        if (lMatches > 1)
        {
            OutError = OpaaxString::Format("{} entities are named '{}': name one by its id", lMatches, lText);
            return Entity{};
        }
        return lFound;
    }

    nlohmann::json DescribeEntity(Entity InEntity, const ComponentRegistry& InTypes, const bool bInWithValues)
    {
        nlohmann::json lJson = nlohmann::json::object();
        if (const EntityMeta* lMeta = InEntity.TryGet<EntityMeta>())
        {
            lJson["id"]     = lMeta->Id.ToString();
            lJson["name"]   = lMeta->Name;
            lJson["parent"] = lMeta->Parent.IsValid() ? nlohmann::json(lMeta->Parent.ToString()) : nlohmann::json();
            lJson["map"]    = lMeta->OwnerMap.IsValid() ? nlohmann::json(lMeta->OwnerMap.CStr()) : nlohmann::json();
        }

        const EntityRegistry& lRegistry = InEntity.GetWorld()->GetRegistry();
        nlohmann::json lNames  = nlohmann::json::array();
        nlohmann::json lValues = nlohmann::json::object();
        InTypes.ForEach([&](const IComponentEntry& InEntry)
        {
            if (!InEntry.Has(lRegistry, InEntity.GetHandle()))
            {
                return;
            }

            lNames.push_back(InEntry.GetName().CStr());
            if (bInWithValues)
            {
                lValues[InEntry.GetName().CStr()] = InEntry.Save(lRegistry, InEntity.GetHandle());
            }
        });

        lJson["components"] = bInWithValues ? lValues : lNames;
        return lJson;
    }

    bool PatchComponent(Entity InEntity, const ComponentRegistry& InTypes, const nlohmann::json& InParams,
                        OpaaxString& OutError)
    {
        const OpaaxString      lType  = InParams.value("type", OpaaxString());
        const IComponentEntry* lEntry = InTypes.FindByName(OpaaxStringID(lType));
        if (lEntry == nullptr)
        {
            OutError = "unknown component type '" + lType + "'";
            return false;
        }

        const auto lValue = InParams.find("value");
        if (lValue != InParams.end() && !lValue->is_object())
        {
            OutError = "\"value\" is an object of the fields to change";
            return false;
        }

        EntityRegistry& lRegistry = InEntity.GetWorld()->GetRegistry();
        nlohmann::json  lData     = lEntry->Has(lRegistry, InEntity.GetHandle())
                                        ? lEntry->Save(lRegistry, InEntity.GetHandle())
                                        : nlohmann::json::object();
        if (lData.is_null())
        {
            lData = nlohmann::json::object();
        }
        if (lValue != InParams.end())
        {
            lData.merge_patch(*lValue);
        }

        lEntry->Load(lRegistry, InEntity.GetHandle(), lData);
        return true;
    }
}
