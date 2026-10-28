#include "Automation/EngineAutomationCommands.h"

#include "Application/OpaaxApplication.h"
#include "Application/Services/IConfigSystem.h"
#include "Application/Services/IEngine.h"
#include "Application/Services/IPaths.h"
#include "Application/Services/IWindowManager.h"
#include "Core/Algo/Algo.h"
#include "Core/IO/FileIO.h"
#include "Core/IO/FileSystem.h"
#include "Core/Maths/Maths.h"
#include "Core/String/OpaaxStringJson.h"
#include "Core/Time/Clock.h"
#include "Engine/Config/Config.h"
#include "Engine/Config/Config_Engine.h"
#include "Engine/FrameClock.h"
#include "Engine/FramePacer.h"
#include "Engine/GameInstance/GameInstance.h"
#include "Engine/GameInstance/GameInstanceManager.h"
#include "Engine/Registries/EngineRegistries.h"
#include "Input/InputKeyNames.h"
#include "Input/InputManager.h"
#include "Platform/Window/Window.h"
#include "Renderer/Camera/CameraComponent.h"
#include "Renderer/Camera/CameraInterpolationComponent.h"
#include "Renderer/Camera/CameraManager.h"
#include "UI/UICanvas.h"
#include "UI/UISubsystem.h"
#include "UI/UIWidget.h"
#include "World/Behaviour/BehaviourSubsystem.h"
#include "World/Components/ComponentRegistry.h"
#include "World/Components/TransformComponent.h"
#include "World/Entity/Entity.h"
#include "World/Entity/EntityHierarchy.h"
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
                    { "type",    InWidget.GetClass().GetName().CStr() },
                    { "visible", IsShown(InWidget) },
                    { "x",       lCentre.x },
                    { "y",       lCentre.y },
                    { "width",   lBounds.HalfExtent.x * 2.f / lPerPixel },
                    { "height",  lBounds.HalfExtent.y * 2.f / lPerPixel } });
            }

            for (const TStrongObjectPtr<UIWidget>& lChild : InWidget.GetChildren())
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

        /** The registered config named InName (its file's name: "Engine"), or null with OutError naming them. */
        Config* FindConfig(const OpaaxString& InName, OpaaxString& OutError)
        {
            OpaaxString lNames;
            for (const TStrongObjectPtr<Config>& lConfig : OpaaxApplication::GetAppService<IConfigSystem>().GetConfigs())
            {
                const OpaaxString lName = Config::NameFromFile(lConfig->FileName()).ToString();
                if (lName == InName)
                {
                    return lConfig.Get();
                }
                lNames += lNames.IsEmpty() ? lName : ", " + lName;
            }

            OutError = "no config '" + InName + "': the configs are " + lNames;
            return nullptr;
        }

        /** A config's values as its file holds them: JSON, or the text itself for a format of its own. */
        nlohmann::json ConfigValues(const Config& InConfig)
        {
            const OpaaxString    lText   = InConfig.ToText();
            const nlohmann::json lValues = nlohmann::json::parse(lText.CStr(), nullptr, false);
            return lValues.is_discarded() ? nlohmann::json(lText) : lValues;
        }

        /** The engine's camera manager, or null with OutError. */
        CameraManager* Cameras(IEngine& InEngine, OpaaxString& OutError)
        {
            CameraManager* lCameras = InEngine.GetCameras();
            if (lCameras == nullptr)
            {
                OutError = "this app has no camera manager";
            }
            return lCameras;
        }

        /** An entity's name and id, as the camera commands give them. */
        nlohmann::json NameAndId(Entity InEntity)
        {
            const EntityMeta* lMeta = InEntity.TryGet<EntityMeta>();
            return nlohmann::json{ { "name", lMeta != nullptr ? lMeta->Name : OpaaxString("<unnamed>") },
                                   { "id",   lMeta != nullptr ? nlohmann::json(lMeta->Id.ToString()) : nlohmann::json() } };
        }

        nlohmann::json SummaryToJson(const CameraTraceSummary& InSummary)
        {
            return nlohmann::json{
                { "frames",            InSummary.Frames },
                { "targetDrift",       InSummary.TargetDrift },
                { "targetDriftFrame",  InSummary.TargetDriftFrame },
                { "motionJitter",      InSummary.MotionJitter },
                { "motionJitterFrame", InSummary.MotionJitterFrame },
                { "largestMove",       InSummary.LargestMove },
                { "largestMoveFrame",  InSummary.LargestMoveFrame },
                { "timeReversals",     InSummary.TimeReversals },
                { "stillMoves",        InSummary.StillMoves },
                { "zoomReversals",     InSummary.ZoomReversals },
                { "cameraSwitches",    InSummary.CameraSwitches },
                { "mostSteps",         InSummary.MostSteps },
                { "fewestSteps",       InSummary.FewestSteps },
                { "stepsSpread",       InSummary.MostSteps - InSummary.FewestSteps } };
        }

        /** A trace and its summary, as the .camera.json file holds them. */
        nlohmann::json TraceToJson(const CameraTraceRecording& InTrace, const CameraTraceSummary& InSummary)
        {
            nlohmann::json lSamples = nlohmann::json::array();
            for (Uint64 lIndex = 0; lIndex < InTrace.Samples.size(); ++lIndex)
            {
                const CameraTraceSample& lSample = InTrace.Samples[lIndex];

                nlohmann::json lFrame{
                    { "frame",     lIndex },
                    { "delta",     lSample.FrameDelta },
                    { "steps",     lSample.Steps },
                    { "alpha",     lSample.Alpha },
                    { "stepDelta", lSample.StepDelta },
                    { "gameTime",  lSample.GameTime },
                    { "camera",    lSample.Camera },
                    { "view",      { { "x", lSample.ViewPosition.x }, { "y", lSample.ViewPosition.y },
                                     { "size", lSample.ViewSize } } } };
                if (lSample.bHasTarget)
                {
                    lFrame["target"] = { { "x", lSample.TargetPosition.x }, { "y", lSample.TargetPosition.y } };
                }
                lSamples.push_back(Move(lFrame));
            }

            return nlohmann::json{
                { "name",      InTrace.Name },
                { "target",    InTrace.Target },
                { "heightPx",  InTrace.HeightPx },
                { "pacing",    InTrace.Pacing },
                { "fixedRate", InTrace.FixedRate },
                { "truncated", InTrace.bTruncated },
                { "summary",   SummaryToJson(InSummary) },
                { "samples",   Move(lSamples) } };
        }

        /**
         * Checks the numbers of InValues against InLimits, each the most it may be ({"targetDrift": 0.01}).
         * False with OutError naming each value past its limit (and its frame), or a limit naming nothing.
         */
        bool CheckLimits(const nlohmann::json& InValues, const nlohmann::json& InLimits, OpaaxString& OutError)
        {
            for (const auto& lLimit : InLimits.items())
            {
                const OpaaxString lName(lLimit.key().c_str());
                const auto        lValue = InValues.find(lLimit.key());
                const OpaaxString lSeparator = OutError.IsEmpty() ? OpaaxString() : OpaaxString("; ");

                if (lValue == InValues.end() || !lValue->is_number() || !lLimit.value().is_number())
                {
                    OutError += lSeparator + "no measure '" + lName + "' to limit: " + OpaaxString(InValues.dump().c_str());
                    continue;
                }

                if (lValue->get<double>() > lLimit.value().get<double>())
                {
                    OutError += lSeparator + OpaaxString::Format("{} is {}, past {}", lName, lValue->get<double>(),
                                                                 lLimit.value().get<double>());

                    const auto lFrame = InValues.find(lLimit.key() + "Frame");
                    if (lFrame != InValues.end())
                    {
                        OutError += OpaaxString::Format(" (frame {})", lFrame->get<Uint64>());
                    }
                }
            }
            return OutError.IsEmpty();
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
            InRunner.Register("app.info", "The app's frame and folders: {frame, workspace, engine, project, assets, save}.",
                [&InHost](const nlohmann::json&)
                {
                    const IPaths& lPaths = OpaaxApplication::GetAppService<IPaths>();
                    return AutomationResult::Ok(nlohmann::json{
                        { "frame",     InHost.GetFrameIndex() },
                        { "workspace", lPaths.WorkspaceRoot() },
                        { "engine",    lPaths.EngineRoot() },
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
                        { "name",     lWorld->GetName().CStr() },
                        { "mode",     ToString(lWorld->GetMode()) },
                        { "paused",   InEngine.GetWorldManager().IsPaused() },
                        { "entities", lWorld->GetEntityCount() },
                        { "camera",   { { "x", lView.Position.x }, { "y", lView.Position.y }, { "size", lView.OrthoSize } } } });
                });

            InRunner.Register("world.pause",
                "Pauses the playing world ({paused}: true) or resumes it (false): a paused world's subsystems do not "
                "tick. Answers {paused}.",
                [&InEngine](const nlohmann::json& InParams)
                {
                    const auto lPaused = InParams.find("paused");
                    if (lPaused == InParams.end() || !lPaused->is_boolean())
                    {
                        return AutomationResult::Fail("give {paused}: true pauses the world, false resumes it");
                    }

                    InEngine.GetWorldManager().SetPaused(lPaused->get<bool>());
                    return AutomationResult::Ok(nlohmann::json{ { "paused", InEngine.GetWorldManager().IsPaused() } });
                });

            InRunner.Register("world.step",
                "Runs one frame of the paused world: its update and the fixed steps that frame owes. The next request "
                "runs after that frame.",
                [&InEngine](const nlohmann::json&)
                {
                    if (!InEngine.GetWorldManager().IsPaused())
                    {
                        return AutomationResult::Fail("the world is not paused: pause it first (world.pause)");
                    }

                    InEngine.GetWorldManager().RequestStep();

                    AutomationResult lResult;
                    lResult.WaitFrames = 1;
                    return lResult;
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
        // Time: the frames a run is given
        // =========================================================================
        void RegisterTime(AutomationRunner& InRunner, IEngine& InEngine)
        {
            InRunner.Register("time.pace",
                "Gives the frames the time of a screen rate from the next frame, whatever the machine: {pacing} as "
                "--frame-pacing reads it (\"144\", \"60,jitter=0.25\", \"60,hitch=0.1@2\", \"deltas=0.016/0.033\"); "
                "\"off\" gives the clock back. Answers {pacing}.",
                [&InEngine](const nlohmann::json& InParams)
                {
                    const OpaaxString lText = InParams.value("pacing", OpaaxString());
                    if (lText.IsEmpty())
                    {
                        return AutomationResult::Fail("give a pacing, like {\"pacing\": \"144\"} (\"off\" for the clock)");
                    }

                    FramePacing lPacing;
                    OpaaxString lError;
                    if (!FramePacing::Parse(lText, lPacing, lError))
                    {
                        return AutomationResult::Fail(lError);
                    }

                    InEngine.SetFramePacing(lPacing);
                    return AutomationResult::Ok(nlohmann::json{ { "pacing", lPacing.ToString() } });
                });
        }

        // =========================================================================
        // Camera: traces of what it framed, measured
        // =========================================================================
        void RegisterCamera(AutomationRunner& InRunner, IEngine& InEngine)
        {
            InRunner.Register("camera.info",
                "How the active world is framed: {world, mode, paused, view, fixedRate, pacing, tracing, clock (fixedDelta, "
                "refreshHz snapped to, snapping, maxSteps, droppedSteps), cameras} with "
                "each camera's {name, id, priority, x, y, size, framed (the view it gives on that frame: x, y, size), "
                "recorded (framed between its steps)}, and in a "
                "Play world the last frame's {live: {name, id, count, tied}, frame: {delta, steps, alpha, interpolated, "
                "stepDelta, gameTime}}. An Edit world is framed by the editor's camera.",
                [&InEngine](const nlohmann::json&)
                {
                    OpaaxString    lError;
                    World* const   lWorld   = ActiveWorld(InEngine, lError);
                    CameraManager* lCameras = lWorld != nullptr ? Cameras(InEngine, lError) : nullptr;
                    if (lCameras == nullptr)
                    {
                        return AutomationResult::Fail(lError);
                    }

                    // Each camera's view as the last frame would have framed it with that camera.
                    const CameraFrame&     lFrame  = lCameras->GetLastFrame();
                    const TOptional<float> lAlpha  = (lFrame.World == lWorld->GetHandle() && lFrame.bInterpolated)
                                                         ? TOptional<float>(static_cast<float>(lFrame.Alpha))
                                                         : NullOpt;

                    nlohmann::json lList = nlohmann::json::array();
                    lWorld->Each<TransformComponent, CameraComponent>(
                        [&](const EntityID InEntity, TransformComponent&, CameraComponent& InCamera)
                        {
                            const Entity     lEntity(InEntity, lWorld);
                            const Vector2F   lAt     = EntityHierarchy::WorldTransform(lEntity).Position;
                            const CameraView lFramed = CameraManager::ResolveCamera(lEntity, lAlpha);

                            nlohmann::json lCamera = NameAndId(lEntity);
                            lCamera["priority"] = InCamera.Priority;
                            lCamera["x"]        = lAt.x;
                            lCamera["y"]        = lAt.y;
                            lCamera["size"]     = InCamera.OrthoSize;
                            lCamera["framed"]   = { { "x", lFramed.Position.x }, { "y", lFramed.Position.y },
                                                    { "size", lFramed.OrthoSize } };
                            lCamera["recorded"] = lEntity.Has<CameraInterpolationComponent>();
                            lList.push_back(Move(lCamera));
                        });

                    const FrameClock& lClock    = InEngine.GetFrameClock();
                    const double      lRefresh  = lClock.GetRefreshInterval();
                    const CameraView& lView     = lWorld->GetCameraView();
                    nlohmann::json    lAnswer{
                        { "world",     lWorld->GetName().CStr() },
                        { "mode",      ToString(lWorld->GetMode()) },
                        { "paused",    InEngine.GetWorldManager().IsPaused() },
                        { "view",      { { "x", lView.Position.x }, { "y", lView.Position.y }, { "size", lView.OrthoSize } } },
                        { "fixedRate", OpaaxApplication::GetAppService<IConfigSystem>().Get<Config_Engine>().GetData().Time.FixedRate },
                        { "pacing",    InEngine.GetFramePacing().ToString() },
                        { "tracing",   lCameras->GetTraceName() },
                        { "clock",     { { "fixedDelta", lClock.GetFixedDelta() },
                                         { "refreshHz", lRefresh > 0.0 ? 1.0 / lRefresh : 0.0 },
                                         { "snapping", lClock.IsSnapping() },
                                         { "maxSteps", lClock.GetMaxStepsPerFrame() },
                                         { "droppedSteps", lClock.GetDroppedSteps() } } },
                        { "cameras",   Move(lList) } };

                    // The last frame drawn of this world, when the camera manager framed it.
                    if (lWorld->GetMode() == EWorldMode::Play && lFrame.World == lWorld->GetHandle())
                    {
                        nlohmann::json lLive = lFrame.Resolution.Count > 0 ? NameAndId(Entity(lFrame.Resolution.Entity, lWorld))
                                                                           : nlohmann::json::object();
                        lLive["count"] = lFrame.Resolution.Count;
                        lLive["tied"]  = lFrame.Resolution.Tied;

                        lAnswer["live"]  = Move(lLive);
                        lAnswer["frame"] = nlohmann::json{
                            { "delta",        lFrame.FrameDelta },
                            { "steps",        lFrame.Steps },
                            { "alpha",        lFrame.Alpha },
                            { "interpolated", lFrame.bInterpolated },
                            { "stepDelta",    lFrame.StepDelta },
                            { "gameTime",     lFrame.GameTime } };
                    }

                    return AutomationResult::Ok(lAnswer);
                });

            InRunner.Register("camera.trace.start",
                "Records every frame of the playing world from the next one: the frame's time, the view, and where "
                "{target} (an entity's name, optional) is drawn; {name} names the files. {heightPx}: the view's height "
                "the pixels are measured in (the window's by default).",
                [&InEngine](const nlohmann::json& InParams)
                {
                    OpaaxString    lError;
                    CameraManager* lCameras = Cameras(InEngine, lError);
                    if (lCameras == nullptr)
                    {
                        return AutomationResult::Fail(lError);
                    }

                    CameraTraceRecording lSetup;
                    lSetup.Name   = InParams.value("name", OpaaxString());
                    lSetup.Target = InParams.value("target", OpaaxString());
                    if (lSetup.Name.IsEmpty())
                    {
                        return AutomationResult::Fail("give the trace a {name}: its files are <name>.camera.json and .csv");
                    }

                    const Window* lWindow = OpaaxApplication::GetAppService<IWindowManager>().GetMainWindow();
                    lSetup.HeightPx  = InParams.value("heightPx", lWindow != nullptr ? static_cast<float>(lWindow->GetHeight()) : 720.f);
                    lSetup.Pacing    = InEngine.GetFramePacing().ToString();
                    lSetup.FixedRate = OpaaxApplication::GetAppService<IConfigSystem>().Get<Config_Engine>().GetData().Time.FixedRate;
                    if (!(lSetup.HeightPx > 0.f))
                    {
                        return AutomationResult::Fail("heightPx is the view's height in pixels, above 0");
                    }

                    lCameras->StartTrace(lSetup);
                    return AutomationResult::Ok(nlohmann::json{ { "name", lSetup.Name }, { "target", lSetup.Target },
                                                                { "heightPx", lSetup.HeightPx } });
                });

            InRunner.Register("camera.trace.stop",
                "Ends the trace, writes <name>.camera.json and <name>.camera.csv in the working folder and answers "
                "{name, frames, files, summary}: the target's largest move on screen (targetDrift), the largest change "
                "of the view's move (motionJitter), largestMove (pixels), timeReversals, stillMoves, zoomReversals, "
                "cameraSwitches, and the most and fewest steps a frame took (mostSteps, fewestSteps, stepsSpread). {limits}, like {\"targetDrift\": 0.01}, fails the request past any of them.",
                [&InEngine](const nlohmann::json& InParams)
                {
                    OpaaxString    lError;
                    CameraManager* lCameras = Cameras(InEngine, lError);
                    if (lCameras == nullptr)
                    {
                        return AutomationResult::Fail(lError);
                    }

                    CameraTraceRecording lTrace;
                    if (!lCameras->StopTrace(lTrace))
                    {
                        return AutomationResult::Fail("no camera trace is recording: start one with camera.trace.start");
                    }

                    const CameraTraceSummary lSummary = CameraTrace::Summarize(lTrace.Samples, lTrace.HeightPx);
                    const OpaaxString        lJson    = FileSystem::MakeAbsolute(lTrace.Name + ".camera.json");
                    const OpaaxString        lCsv     = FileSystem::MakeAbsolute(lTrace.Name + ".camera.csv");

                    if (!FileIO::WriteAllText(lJson, OpaaxString(TraceToJson(lTrace, lSummary).dump(1).c_str()))
                        || !FileIO::WriteAllText(lCsv, CameraTrace::ToCsv(lTrace.Samples, lTrace.HeightPx)))
                    {
                        return AutomationResult::Fail("cannot write the trace's files at '" + lJson + "'");
                    }

                    const nlohmann::json lMeasures = SummaryToJson(lSummary);
                    const auto           lLimits   = InParams.find("limits");
                    if (lLimits != InParams.end() && !CheckLimits(lMeasures, *lLimits, lError))
                    {
                        return AutomationResult::Fail(lError + " — see " + lCsv);
                    }

                    return AutomationResult::Ok(nlohmann::json{
                        { "name",      lTrace.Name },
                        { "frames",    lTrace.Samples.size() },
                        { "truncated", lTrace.bTruncated },
                        { "files",     { { "json", lJson }, { "csv", lCsv } } },
                        { "summary",   lMeasures } });
                });

            InRunner.Register("camera.trace.compare",
                "How far the stopped trace {trace} is from where the stopped trace {reference} says its camera was at "
                "each of its game times (the reference's frame then, or both ends of the step blended): {shared, "
                "position, size, atTime}, world units. The reference is a steady pacing at the fixed rate or a multiple, "
                "so one level played at any rate must match it. {limits}, like {\"position\": 0.001}, fails past them.",
                [&InEngine](const nlohmann::json& InParams)
                {
                    OpaaxString    lError;
                    CameraManager* lCameras = Cameras(InEngine, lError);
                    if (lCameras == nullptr)
                    {
                        return AutomationResult::Fail(lError);
                    }

                    const OpaaxString           lReferenceName = InParams.value("reference", OpaaxString());
                    const OpaaxString           lTraceName     = InParams.value("trace", OpaaxString());
                    const CameraTraceRecording* lReference     = lCameras->FindTrace(lReferenceName);
                    const CameraTraceRecording* lTrace         = lCameras->FindTrace(lTraceName);
                    if (lReference == nullptr || lTrace == nullptr)
                    {
                        return AutomationResult::Fail("no stopped trace '" + (lReference == nullptr ? lReferenceName : lTraceName)
                                                      + "': stop one under that name first (camera.trace.stop)");
                    }

                    const CameraTraceGap lGap      = CameraTrace::Compare(lReference->Samples, lTrace->Samples);
                    const nlohmann::json lMeasures = nlohmann::json{
                        { "reference", lReferenceName }, { "trace", lTraceName }, { "shared", lGap.Shared },
                        { "position", lGap.Position }, { "size", lGap.Size }, { "atTime", lGap.AtTime } };

                    if (lGap.Shared == 0)
                    {
                        return AutomationResult::Fail("the reference '" + lReferenceName + "' places no frame of '" + lTraceName
                                                      + "': same camera, and a reference drawing every step's end");
                    }

                    const auto lLimits = InParams.find("limits");
                    if (lLimits != InParams.end() && !CheckLimits(lMeasures, *lLimits, lError))
                    {
                        return AutomationResult::Fail(lError);
                    }

                    return AutomationResult::Ok(lMeasures);
                });
        }

        // =========================================================================
        // Configs: read, and changed for this run
        // =========================================================================
        void RegisterConfig(AutomationRunner& InRunner)
        {
            InRunner.Register("config.get",
                "A config's values as its file holds them: {config}, its file's name (\"Engine\", \"Renderer\"...). "
                "Answers {config, values}.",
                [](const nlohmann::json& InParams)
                {
                    const OpaaxString lName = InParams.value("config", OpaaxString());
                    OpaaxString       lError;
                    const Config*     lConfig = FindConfig(lName, lError);
                    if (lConfig == nullptr)
                    {
                        return AutomationResult::Fail(lError);
                    }

                    return AutomationResult::Ok(nlohmann::json{ { "config", lName }, { "values", ConfigValues(*lConfig) } });
                });

            InRunner.Register("config.set",
                "Merges {values}, shaped like the config's file ({\"Time\": {\"FixedRate\": 30}}), into {config} for "
                "this run, without saving; applied at once, as the Config panel does. A value the config does not have, "
                "or of the wrong type, is refused. Answers {config, values} after.",
                [](const nlohmann::json& InParams)
                {
                    const OpaaxString lName = InParams.value("config", OpaaxString());
                    OpaaxString       lError;
                    Config*           lConfig = FindConfig(lName, lError);
                    if (lConfig == nullptr)
                    {
                        return AutomationResult::Fail(lError);
                    }

                    const auto lValues = InParams.find("values");
                    if (lValues == InParams.end() || !lValues->is_object())
                    {
                        return AutomationResult::Fail("give {values} shaped like the file, like {\"Time\": {\"FixedRate\": 30}}");
                    }

                    if (!lConfig->MergeText(OpaaxString(lValues->dump().c_str()), lError))
                    {
                        return AutomationResult::Fail(lError);
                    }

                    return AutomationResult::Ok(nlohmann::json{ { "config", lName }, { "values", ConfigValues(*lConfig) } });
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
        RegisterTime(InRunner, InEngine);
        RegisterConfig(InRunner);
        RegisterCamera(InRunner, InEngine);
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
