#include "Automation/EngineAutomationCommands.h"

#include "Application/OpaaxApplication.h"
#include "Application/Services/IConfigSystem.h"
#include "Application/Services/IEngine.h"
#include "Application/Services/IPaths.h"
#include "Application/Services/IWindowManager.h"
#include "Automation/InputAutomationCommands.h"
#include "Automation/OnlineAutomationCommands.h"
#include "Automation/SaveAutomationCommands.h"
#include "Automation/SettingsAutomationCommands.h"
#include "Core/Algo/Algo.h"
#include "Core/Build/BuildInfo.h"
#include "Core/IO/FileIO.h"
#include "Core/IO/FileSystem.h"
#include "Core/Maths/Maths.h"
#include "Core/String/OpaaxStringJson.h"
#include "Core/Time/Clock.h"
#include "Engine/Config/Config.h"
#include "Engine/Config/Config_Engine.h"
#include "Engine/FrameClock.h"
#include "Engine/FrameLimiter.h"
#include "Engine/FramePacer.h"
#include "Engine/GameInstance/GameInstance.h"
#include "Engine/GameInstance/GameInstanceManager.h"
#include "Engine/Registries/EngineRegistries.h"
#include "Input/InputKeyNames.h"
#include "Input/InputManager.h"
#include "Platform/Window/Window.h"
#include "Renderer/Camera/CameraComponent.h"
#include "Renderer/Camera/CameraInterpolationComponent.h"
#include "Renderer/Camera/CameraDirector.h"
#include "Renderer/Camera/CameraConfinerStage.h"
#include "Renderer/Camera/CameraFollowStage.h"
#include "Renderer/Camera/CameraGroupStage.h"
#include "Renderer/Camera/CameraManager.h"
#include "Renderer/Camera/CameraRigSubsystem.h"
#include "Renderer/Camera/CameraSpeedZoomStage.h"
#include "Renderer/Camera/CameraZoneStage.h"
#include "Renderer/Camera/CameraShakeSourceComponent.h"
#include "Renderer/Camera/CameraShakeSubsystem.h"
#include "UI/UIButtonBase.h"
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
            EKeyCode lKey = EKeyCode::None;
            if (!KeyFromName(InName, lKey) || !IsKeyCodeBindable(lKey))
            {
                return false;
            }
            OutKey = lKey;
            return true;
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

                nlohmann::json lRow{
                    { "name",    InWidget.Name },
                    { "type",    InWidget.GetClass().GetName().CStr() },
                    { "visible", IsShown(InWidget) },
                    { "x",       lCentre.x },
                    { "y",       lCentre.y },
                    { "width",   lBounds.HalfExtent.x * 2.f / lPerPixel },
                    { "height",  lBounds.HalfExtent.y * 2.f / lPerPixel },
                    { "quads",   InWidget.GetQuads().size() } };

                // Any button, the engine's or a game's, answers the state it shows.
                if (InWidget.IsA<UIButtonBase>())
                {
                    lRow["state"] = ToString(static_cast<const UIButtonBase&>(InWidget).GetState());
                }

                OutList.push_back(Move(lRow));
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

        /** A config's text as JSON, or the text itself for a format of its own. */
        nlohmann::json ConfigJson(const OpaaxString& InText)
        {
            const nlohmann::json lValues = nlohmann::json::parse(InText.CStr(), nullptr, false);
            return lValues.is_discarded() ? nlohmann::json(InText) : lValues;
        }

        /**
         * A config's answer: {config, values} what the engine reads, {project} its file's values and
         * {player} the player's over them.
         */
        nlohmann::json DescribeConfig(const OpaaxString& InName, const Config& InConfig)
        {
            return nlohmann::json{ { "config",  InName },
                                   { "values",  ConfigJson(InConfig.ToEffectiveText()) },
                                   { "project", ConfigJson(InConfig.ToText()) },
                                   { "player",  ConfigJson(InConfig.GetPlayerText()) } };
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

        /** A view as the commands give it: {x, y, size, rotation}. */
        nlohmann::json ViewToJson(const CameraView& InView)
        {
            return nlohmann::json{ { "x", InView.Position.x }, { "y", InView.Position.y }, { "size", InView.OrthoSize },
                                   { "rotation", InView.Rotation } };
        }

        /** An entity's name and id, as the camera commands give them. */
        nlohmann::json NameAndId(Entity InEntity)
        {
            const EntityMeta* lMeta = InEntity.TryGet<EntityMeta>();
            return nlohmann::json{ { "name", lMeta != nullptr ? lMeta->Name : OpaaxString("<unnamed>") },
                                   { "id",   lMeta != nullptr ? nlohmann::json(lMeta->Id.ToString()) : nlohmann::json() } };
        }

        nlohmann::json BoundsToJson(const TOptional<Bounds2D>& InBounds)
        {
            return InBounds.has_value() ? nlohmann::json{ { "center", InBounds->Center }, { "halfExtent", InBounds->HalfExtent } }
                                        : nlohmann::json();
        }

        /**
         * InCamera's rig as camera.info gives it: each stage in the order it runs, {class, stage}, and
         * what it holds (a follow's dead zone and lead, a confiner's or a group's bounds, the zone and its
         * blend, a speed zoom's size). Empty without a rig.
         */
        nlohmann::json StagesToJson(World& InWorld, const EntityID InCamera)
        {
            nlohmann::json            lList = nlohmann::json::array();
            const CameraRigSubsystem* lRig  = InWorld.GetSubsystems().GetSubsystem<CameraRigSubsystem>();
            if (lRig == nullptr)
            {
                return lList;
            }

            for (CameraStage* lStage : lRig->GetStages(InCamera))
            {
                nlohmann::json lJson{ { "class", lStage->GetClass().GetName().CStr() }, { "stage", ToString(lStage->GetStage()) } };
                if (const CameraFollowStage* lFollow = Cast<CameraFollowStage>(lStage))
                {
                    lJson["target"]     = lFollow->Target.CStr();
                    lJson["inDeadZone"] = lFollow->GetState().bInDeadZone;
                    lJson["lead"]       = lFollow->GetState().Lead;
                }
                else if (CameraConfinerStage* lConfiner = Cast<CameraConfinerStage>(lStage))
                {
                    lJson["bounds"] = BoundsToJson(lConfiner->GetBounds());
                }
                else if (const CameraGroupStage* lGroup = Cast<CameraGroupStage>(lStage))
                {
                    lJson["bounds"] = BoundsToJson(lGroup->GetTargetBounds());
                }
                else if (const CameraZoneStage* lZoneStage = Cast<CameraZoneStage>(lStage))
                {
                    const CameraZoneState& lState = lZoneStage->GetState();
                    const Entity           lZone  = lState.Zone.has_value() ? Entity(lState.Zone->Entity, &InWorld) : Entity();
                    lJson["zone"]  = lZone.IsValid() ? nlohmann::json(lZone.GetName().CStr()) : nlohmann::json();
                    lJson["blend"] = CameraZoneBlendWeight(lState);
                }
                else if (const CameraSpeedZoomStage* lZoom = Cast<CameraSpeedZoomStage>(lStage))
                {
                    lJson["size"] = lZoom->GetState().Size;
                }
                lList.push_back(Move(lJson));
            }
            return lList;
        }

        /** The active world's shakes, or null with OutError: only a Play world has them. */
        CameraShakeSubsystem* Shakes(IEngine& InEngine, OpaaxString& OutError)
        {
            World* const          lWorld  = ActiveWorld(InEngine, OutError);
            CameraShakeSubsystem* lShakes = lWorld != nullptr ? lWorld->GetSubsystems().GetSubsystem<CameraShakeSubsystem>()
                                                              : nullptr;
            if (lWorld != nullptr && lShakes == nullptr)
            {
                OutError = "camera shakes play in a Play world: level.play first";
            }
            return lShakes;
        }

        /** The shakes under way as camera.info gives them, their trauma at InGameTime. */
        nlohmann::json ShakesToJson(World& InWorld, const CameraShakeSubsystem& InShakes, const double InGameTime)
        {
            nlohmann::json lList = nlohmann::json::array();
            for (const CameraShakePlaying& lPlaying : InShakes.GetPlaying())
            {
                const Entity lSource(lPlaying.Source, &InWorld);
                nlohmann::json lShake{
                    { "id",      lPlaying.Id },
                    { "shake",   lPlaying.Owned ? nlohmann::json() : nlohmann::json(lPlaying.Asset.GetPath()) },
                    { "scale",   lPlaying.Scale },
                    { "seed",    lPlaying.Seed },
                    { "start",   lPlaying.Start },
                    { "stopped", lPlaying.StoppedAt.has_value() ? nlohmann::json(*lPlaying.StoppedAt) : nlohmann::json() },
                    { "source",  lSource.IsValid() ? NameAndId(lSource)["name"] : nlohmann::json() } };
                if (lPlaying.bTrauma)
                {
                    lShake["trauma"] = lPlaying.Trauma.At(InGameTime);
                }
                lList.push_back(Move(lShake));
            }
            return lList;
        }

        /** How a camera's view moves against the steps, as camera.info gives it ("unknown" when not recorded). */
        const char* RhythmOf(Entity InCamera)
        {
            const CameraInterpolationComponent* lHistory = InCamera.TryGet<CameraInterpolationComponent>();
            return ToString(lHistory != nullptr ? lHistory->Rhythm.Last : ECameraRhythm::Unknown);
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
                { "switchJump",        InSummary.SwitchJump },
                { "switchJumpFrame",   InSummary.SwitchJumpFrame },
                { "cuts",              InSummary.Cuts },
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
                    { "cut",       lSample.bCut },
                    { "camera",    lSample.Camera },
                    { "view",      { { "x", lSample.ViewPosition.x }, { "y", lSample.ViewPosition.y },
                                     { "size", lSample.ViewSize }, { "rotation", lSample.ViewRotation } } } };
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
        void RegisterApp(ICommandRegistry& InCommands, IAutomationHost& InHost)
        {
            InCommands.Add("app.info",
                "The app's frame, build kind and folders: {frame, devBuild, workspace, engine, project, assets, save, user}.",
                [&InHost](const nlohmann::json&)
                {
                    const IPaths& lPaths = OpaaxApplication::GetAppService<IPaths>();
                    return AutomationResult::Ok(nlohmann::json{
                        { "frame",     InHost.GetFrameIndex() },
                        { "devBuild",  Build::IsDevBuild() },
                        { "workspace", lPaths.WorkspaceRoot() },
                        { "engine",    lPaths.EngineRoot() },
                        { "project",   lPaths.ProjectRoot() },
                        { "assets",    lPaths.AssetsDir() },
                        { "save",      lPaths.SaveDir() },
                        { "user",      lPaths.UserDir() } });
                });

            InCommands.Add("app.quit", "Closes the app at the end of this frame.",
                [&InHost](const nlohmann::json&)
                {
                    InHost.RequestQuit();
                    return AutomationResult::Ok();
                });

            InCommands.Add(CommandSpec("expect.quit",
                "Holds the queue until the game closes the app itself (QuitGame), then answers ok; fails if the "
                "app is still open after {seconds} (default 10, real time).")
                    .Param("seconds", ECommandParamType::Float, "real seconds to wait (10)").Optional(),
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

            InCommands.Add(CommandSpec("screenshot",
                "Saves this frame as a PNG at {path} (relative paths are from the working folder); the next "
                "request runs once it is written.")
                    .Param("path", ECommandParamType::String, "the PNG to write"),
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
        void RegisterInput(ICommandRegistry& InCommands, IEngine& InEngine)
        {
            InCommands.Add(CommandSpec("input.key",
                "Feeds {key} (\"Space\", \"A\", \"Mouse_Left\"...) to the game: {action} \"press\", \"release\" or "
                "\"tap\" (down for {frames}, default 2, then up).")
                    .Param("key", ECommandParamType::String, "the key: Space, A, Mouse_Left...")
                    .Param("action", ECommandParamType::String, "press, release or tap (tap)").Choices({ "press", "release", "tap" }).Optional()
                    .Param("frames", ECommandParamType::Int, "frames a tap holds the key (2)").Optional(),
                [&InEngine](const nlohmann::json& InParams)
                {
                    EKeyCode          lKey = EKeyCode::None;
                    const OpaaxString lName = InParams.value("key", OpaaxString());
                    if (!ParseKey(lName, lKey))
                    {
                        return AutomationResult::Fail("unknown key '" + lName + "'");
                    }

                    if (IsMouseWheel(lKey))
                    {
                        return AutomationResult::Fail("the wheel is never held: turn it with input.scroll {y} "
                                                      "(1 is a notch up, -1 a notch down)");
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

            InCommands.Add(CommandSpec("input.mouse",
                "Moves the game's pointer to {x, y} (pixels of the game view, from its top left).")
                    .Param("x", ECommandParamType::Float, "pixels from the game view's left")
                    .Param("y", ECommandParamType::Float, "pixels from its top"),
                [&InEngine](const nlohmann::json& InParams)
                {
                    InEngine.GetInput().OnMouseMoved(InParams.at("x").get<float>(), InParams.at("y").get<float>());
                    return AutomationResult::Ok();
                });

            InCommands.Add(CommandSpec("input.scroll",
                "Turns the game's mouse wheel by {y} notches (1 up, -1 down) and {x} (default 0) in one frame; "
                "the next request runs after that frame.")
                    .Param("y", ECommandParamType::Float, "notches: 1 up, -1 down").Optional()
                    .Param("x", ECommandParamType::Float, "notches sideways (0)").Optional(),
                [&InEngine](const nlohmann::json& InParams)
                {
                    const auto lY = InParams.find("y");
                    const auto lX = InParams.find("x");
                    if ((lY == InParams.end() || !lY->is_number()) && (lX == InParams.end() || !lX->is_number()))
                    {
                        return AutomationResult::Fail("give the notches as {y} (1 up, -1 down), and {x} for a "
                                                      "sideways wheel");
                    }

                    const float lNotchesX = (lX != InParams.end() && lX->is_number()) ? lX->get<float>() : 0.f;
                    const float lNotchesY = (lY != InParams.end() && lY->is_number()) ? lY->get<float>() : 0.f;
                    InEngine.GetInput().OnMouseScrolled(lNotchesX, lNotchesY);

                    // A notch lasts its frame: the next request reads what that frame did with it.
                    AutomationResult lResult = AutomationResult::Ok();
                    lResult.WaitFrames       = 1;
                    return lResult;
                });
        }

        // =========================================================================
        // The game's UI
        // =========================================================================
        void RegisterUI(ICommandRegistry& InCommands, IEngine& InEngine)
        {
            InCommands.Add(CommandSpec("ui.list",
                "The game UI's widgets, depth-first: [{name, type, visible, x, y, width, height, quads}], x and y "
                "their centre in game-view pixels, quads how many it draws; a button also answers its state "
                "(Normal, Hovered, Pressed, Disabled). Filter: {name}.")
                    .Param("name", ECommandParamType::String, "only the widgets of this name").Optional(),
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

            InCommands.Add(CommandSpec("ui.click",
                "Clicks the game UI's widget named {widget}: the pointer moves to its centre and the left button "
                "is tapped (down for {frames}, default 2).")
                    .Param("widget", ECommandParamType::String, "the widget's name")
                    .Param("frames", ECommandParamType::Int, "frames the button is held (2)").Optional(),
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

            InCommands.Add(CommandSpec("ui.hover",
                "Moves the game's pointer to the centre of the UI widget named {widget}, and answers {x, y} after "
                "the frame that routes the move (the widget is then hovered).")
                    .Param("widget", ECommandParamType::String, "the widget's name"),
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

                    InEngine.GetInput().OnMouseMoved(lPixel.x, lPixel.y);

                    AutomationResult lResult = AutomationResult::Ok(nlohmann::json{ { "x", lPixel.x }, { "y", lPixel.y } });
                    lResult.WaitFrames = 2;   // the move is routed, then the widget rebuilds
                    return lResult;
                });
        }

        // =========================================================================
        // World
        // =========================================================================
        void RegisterWorld(ICommandRegistry& InCommands, IEngine& InEngine)
        {
            InCommands.Add("world.info", "The active world: {name, mode, paused, timeScale, entities, camera}.",
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
                        { "name",      lWorld->GetName().CStr() },
                        { "mode",      ToString(lWorld->GetMode()) },
                        { "paused",    InEngine.GetWorldManager().IsPaused() },
                        { "timeScale", InEngine.GetWorldManager().GetTimeScale() },
                        { "entities",  lWorld->GetEntityCount() },
                        { "camera",    ViewToJson(lView) } });
                });

            InCommands.Add(CommandSpec("world.pause",
                "Pauses the playing world ({paused}: true) or resumes it (false): a paused world's subsystems do not "
                "tick. Answers {paused}.")
                    .Param("paused", ECommandParamType::Bool, "true pauses, false resumes"),
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

            InCommands.Add(CommandSpec("world.step",
                "Runs one frame of the paused world: its update and the fixed steps that frame owes. The next request "
                "runs after that frame.").Cheat(),
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

            InCommands.Add(CommandSpec("entity.list",
                "The active world's entities: [{id, name, parent, map, components}]. Filters: {name}, {component}.")
                    .Param("name", ECommandParamType::String, "only the entities of this name").Optional()
                    .Param("component", ECommandParamType::String, "only the entities with this component").Optional(),
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

            InCommands.Add(CommandSpec("entity.get", "One entity of the active world with its components' values: {entity} (id or name).")
                    .Param("entity", ECommandParamType::String, "the entity's id or name"),
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

            InCommands.Add(CommandSpec("component.set",
                "Changes fields of {entity}'s component {type}: {value} is merged into it (added when missing). "
                "A field of the wrong type changes nothing.")
                    .Param("entity", ECommandParamType::String, "the entity's id or name")
                    .Param("type", ECommandParamType::String, "the component")
                    .Param("value", ECommandParamType::Any, "the fields to merge, a JSON object").Cheat(),
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
        void RegisterLevels(ICommandRegistry& InCommands, IEngine& InEngine)
        {
            InCommands.Add(CommandSpec("level.play",
                "Opens the level at {path} (relative to the project's assets) in a new play world; the next request "
                "runs once it is open.")
                    .Param("path", ECommandParamType::String, "the level, from the assets").Cheat(),
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

            InCommands.Add(CommandSpec("world.wait",
                "Lets the playing world's clock advance {seconds} (game time, whatever the frame rate) before the "
                "next request: at a time scale of 0.25 it lasts four times longer. Ends early if the world changes.")
                    .Param("seconds", ECommandParamType::Float, "game seconds"),
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
        void RegisterTime(ICommandRegistry& InCommands, IEngine& InEngine)
        {
            InCommands.Add(CommandSpec("time.pace",
                "Gives the frames the time of a screen rate from the next frame, whatever the machine: {pacing} as "
                "--frame-pacing reads it (\"144\", \"60,jitter=0.25\", \"60,hitch=0.1@2\", \"deltas=0.016/0.033\"); "
                "\"off\" gives the clock back. Answers {pacing}.")
                    .Param("pacing", ECommandParamType::String, "144, 60,jitter=0.25... or off"),
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

            InCommands.Add(CommandSpec("time.scale",
                "Slows or speeds the game: {scale} seconds of game time for each real one (0.25 a quarter, 1 "
                "normal), kept in [0.05, 4]. The world and its fixed steps run on game time, the UI on real time. "
                "Back to 1 when the game ends. Answers {scale}.")
                    .Param("scale", ECommandParamType::Float, "game seconds per real second")
                    .Cheat()
                    .DevOnly(),
                [&InEngine](const nlohmann::json& InParams)
                {
                    const auto lScale = InParams.find("scale");
                    if (lScale == InParams.end() || !lScale->is_number())
                    {
                        return AutomationResult::Fail("give {scale}, a number: 0.25 is a quarter speed, 1 normal");
                    }

                    WorldManager& lWorlds = InEngine.GetWorldManager();
                    lWorlds.SetTimeScale(lScale->get<float>());
                    return AutomationResult::Ok(nlohmann::json{ { "scale", lWorlds.GetTimeScale() } });
                });
        }

        // =========================================================================
        // Window: its mode and size, the screen's modes, the frame cap
        // =========================================================================
        void RegisterWindow(ICommandRegistry& InCommands, IEngine& InEngine)
        {
            InCommands.Add("window.info",
                "The main window: {mode, width, height, refreshRate (its screen's), decorated, follows (whether it "
                "follows Window.Mode, Width and Height at once: a game's does, the editor's not), modes (its screen's "
                "fullscreen modes, smallest first: {width, height, refreshRate}), frameCap {maxFrameRate (0 when "
                "none holds: no cap, no game running, paced), heldRefreshes (each frame's, by the swap interval; 0 "
                "on the beat), unheld (the screen ignored the swap interval)}}.",
                [&InEngine](const nlohmann::json&)
                {
                    const IWindowManager& lWindows = OpaaxApplication::GetAppService<IWindowManager>();
                    const Window*         lWindow  = lWindows.GetMainWindow();
                    if (lWindow == nullptr)
                    {
                        return AutomationResult::Fail("this app has no main window");
                    }

                    nlohmann::json lModes = nlohmann::json::array();
                    for (const DisplayMode& lMode : lWindow->GetDisplayModes())
                    {
                        lModes.push_back(nlohmann::json{
                            { "width", lMode.Width }, { "height", lMode.Height }, { "refreshRate", lMode.RefreshRate } });
                    }

                    const FrameLimiter& lCap = InEngine.GetFrameLimiter();
                    return AutomationResult::Ok(nlohmann::json{
                        { "mode",        ToString(lWindow->GetWindowMode()) },
                        { "width",       lWindow->GetWidth() },
                        { "height",      lWindow->GetHeight() },
                        { "refreshRate", lWindow->GetRefreshRate() },
                        { "decorated",   lWindow->IsDecorated() },
                        { "follows",     lWindows.IsFollowingEngineConfig() },
                        { "modes",       Move(lModes) },
                        { "frameCap",    nlohmann::json{
                            { "maxFrameRate",  lCap.GetMaxFrameRate() },
                            { "heldRefreshes", lCap.GetHeldRefreshes() },
                            { "unheld",        lCap.IsRefreshUnheld() } } } });
                });
        }

        // =========================================================================
        // Camera: traces of what it framed, measured
        // =========================================================================
        void RegisterCamera(ICommandRegistry& InCommands, IEngine& InEngine)
        {
            InCommands.Add("camera.info",
                "How the active world is framed: {world, mode, paused, view (x, y, size, rotation), fixedRate, pacing, "
                "tracing, clock (fixedDelta, refreshHz snapped to, snapping, maxSteps, droppedSteps), cameras} with "
                "each camera's {name, id, priority, x, y, size, blendIn, blendCurve, framed (the view it gives on that frame), "
                "recorded (framed between its steps), rhythm (over the last 60 steps: still, steps, update, or mixed "
                "when one entity moves it both in the steps and in Update; unknown before 60 steps), stages (its rig, "
                "in the order the stages run: {class, stage (Body, Size, Constraint)} and what each holds: a follow's "
                "target, inDeadZone and lead, a confiner's or a group's bounds {center, halfExtent}, a zone stage's zone "
                "and blend, a speed zoom's size)}, and in a "
                "Play world the last frame's {live: {name, id, rhythm, stages, count, tied, blends (blends under way), blend "
                "(the newest: from, the camera it leaves, null for a view held; weight toward the live camera), "
                "unmodified (the view before the modifiers)}, modifiers (each {class, priority}, in the order they run), "
                "shakes (each under way {id, shake (its asset), scale, seed, start and stopped (game times), source (its "
                "entity's name), trauma (a trauma source's, at the frame)}), "
                "frame: {delta, steps, alpha, interpolated, "
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
                            lCamera["priority"]   = InCamera.Priority;
                            lCamera["x"]          = lAt.x;
                            lCamera["y"]          = lAt.y;
                            lCamera["size"]       = InCamera.OrthoSize;
                            lCamera["framed"]     = ViewToJson(lFramed);
                            lCamera["blendIn"]    = InCamera.BlendIn;
                            lCamera["blendCurve"] = ToString(InCamera.BlendCurve);
                            lCamera["recorded"]   = lEntity.Has<CameraInterpolationComponent>();
                            lCamera["rhythm"]     = RhythmOf(lEntity);
                            lCamera["stages"]     = StagesToJson(*lWorld, InEntity);
                            lList.push_back(Move(lCamera));
                        });

                    const FrameClock& lClock    = InEngine.GetFrameClock();
                    const double      lRefresh  = lClock.GetRefreshInterval();
                    const CameraView& lView     = lWorld->GetCameraView();
                    nlohmann::json    lAnswer{
                        { "world",     lWorld->GetName().CStr() },
                        { "mode",      ToString(lWorld->GetMode()) },
                        { "paused",    InEngine.GetWorldManager().IsPaused() },
                        { "view",      ViewToJson(lView) },
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
                        if (lFrame.Resolution.Count > 0)
                        {
                            lLive["rhythm"] = RhythmOf(Entity(lFrame.Resolution.Entity, lWorld));
                            lLive["stages"] = StagesToJson(*lWorld, lFrame.Resolution.Entity);
                        }
                        lLive["count"]      = lFrame.Resolution.Count;
                        lLive["tied"]       = lFrame.Resolution.Tied;
                        lLive["blends"]     = lFrame.Resolution.Blends;
                        lLive["unmodified"] = ViewToJson(lFrame.Resolution.Unmodified);
                        if (lFrame.Resolution.Blends > 0)
                        {
                            const Entity lFrom(lFrame.Resolution.BlendFrom, lWorld);
                            lLive["blend"] = nlohmann::json{
                                { "from",   lFrom.IsValid() ? NameAndId(lFrom)["name"] : nlohmann::json() },
                                { "weight", lFrame.Resolution.BlendWeight } };
                        }

                        lAnswer["live"]  = Move(lLive);

                        // What changes the view after the blend, in the order it runs.
                        nlohmann::json lModifiers = nlohmann::json::array();
                        if (const CameraDirector* lDirector = lWorld->GetSubsystems().GetSubsystem<CameraDirector>())
                        {
                            for (const TStrongObjectPtr<CameraModifier>& lModifier : lDirector->GetModifiers())
                            {
                                lModifiers.push_back(nlohmann::json{ { "class", lModifier->GetClass().GetName().CStr() },
                                                                     { "priority", lModifier->Priority } });
                            }
                        }
                        lAnswer["modifiers"] = Move(lModifiers);
                        if (const CameraShakeSubsystem* lShakes = lWorld->GetSubsystems().GetSubsystem<CameraShakeSubsystem>())
                        {
                            lAnswer["shakes"] = ShakesToJson(*lWorld, *lShakes, lFrame.GameTime);
                        }
                        lAnswer["frame"] = nlohmann::json{
                            { "delta",        lFrame.FrameDelta },
                            { "steps",        lFrame.Steps },
                            { "alpha",        lFrame.Alpha },
                            { "interpolated", lFrame.bInterpolated },
                            { "stepDelta",    lFrame.StepDelta },
                            { "gameTime",     lFrame.GameTime },
                            { "cut",          lFrame.bCut } };
                    }

                    return AutomationResult::Ok(lAnswer);
                });

            InCommands.Add(CommandSpec("camera.shake",
                "Plays the camera shake asset {path} on the active Play world, times {scale} (1), its noise picked by "
                "{seed} (0), from the next step: answers its {id}. {stop}: a shake's id, faded out from the next step; "
                "{stopAll}: true fades every shake out.")
                    .Param("path", ECommandParamType::String, "the shake asset").Optional()
                    .Param("scale", ECommandParamType::Float, "times its strength (1)").Optional()
                    .Param("seed", ECommandParamType::Int, "picks its noise (0)").Optional()
                    .Param("stop", ECommandParamType::Int, "the id of a shake to stop").Optional()
                    .Param("stopAll", ECommandParamType::Bool, "stops every shake").Optional(),
                [&InEngine](const nlohmann::json& InParams)
                {
                    OpaaxString           lError;
                    CameraShakeSubsystem* lShakes = Shakes(InEngine, lError);
                    if (lShakes == nullptr)
                    {
                        return AutomationResult::Fail(lError);
                    }

                    if (InParams.value("stopAll", false))
                    {
                        lShakes->StopAll();
                        return AutomationResult::Ok(nlohmann::json{ { "stopped", lShakes->GetPlaying().size() } });
                    }

                    const auto lStop = InParams.find("stop");
                    if (lStop != InParams.end())
                    {
                        if (!lStop->is_number_unsigned())
                        {
                            return AutomationResult::Fail("\"stop\" is the id camera.shake answered");
                        }
                        lShakes->Stop(lStop->get<Uint32>());
                        return AutomationResult::Ok(nlohmann::json{ { "stopped", lStop->get<Uint32>() } });
                    }

                    TAssetRef<CameraShake> lShake;
                    lShake.Path = InParams.value("path", OpaaxString());
                    if (lShake.IsEmpty())
                    {
                        return AutomationResult::Fail("\"path\" names a CameraShake asset (Shakes/Hit.opaaxasset); "
                                                      "or give {stop} or {stopAll}");
                    }

                    const Uint32 lId = lShakes->Play(lShake, InParams.value("scale", 1.f), InParams.value("seed", 0u));
                    if (lId == 0)
                    {
                        return AutomationResult::Fail("'" + lShake.Path + "' did not load as a CameraShake: check the path "
                                                      "(asset.get answers its class)");
                    }
                    return AutomationResult::Ok(nlohmann::json{ { "id", lId } });
                });

            InCommands.Add(CommandSpec("camera.trauma",
                "Raises the trauma of {entity} (a CameraShakeSource with bUseTrauma) by {amount} (lowers it, negative), "
                "kept in [0, 1], from the next step: answers its {trauma} then.")
                    .Param("entity", ECommandParamType::String, "the shake source")
                    .Param("amount", ECommandParamType::Float, "trauma to add, negative to lower"),
                [&InEngine](const nlohmann::json& InParams)
                {
                    OpaaxString           lError;
                    CameraShakeSubsystem* lShakes = Shakes(InEngine, lError);
                    if (lShakes == nullptr)
                    {
                        return AutomationResult::Fail(lError);
                    }

                    Entity lEntity = FindEntity(*InEngine.GetWorldManager().GetActiveWorld(), InParams, lError);
                    if (!lEntity.IsValid())
                    {
                        return AutomationResult::Fail(lError);
                    }

                    const CameraShakeSourceComponent* lSource = lEntity.TryGet<CameraShakeSourceComponent>();
                    if (lSource == nullptr || !lSource->bUseTrauma)
                    {
                        return AutomationResult::Fail(InParams.at("entity").get<OpaaxString>() + " is not a trauma source: "
                                                      "give it a CameraShakeSource with bUseTrauma");
                    }

                    const auto lAmount = InParams.find("amount");
                    if (lAmount == InParams.end() || !lAmount->is_number())
                    {
                        return AutomationResult::Fail("\"amount\" is the trauma to add, 0 to 1");
                    }

                    lShakes->AddTrauma(lEntity.GetHandle(), lAmount->get<float>());
                    return AutomationResult::Ok(nlohmann::json{ { "trauma", lShakes->GetTrauma(lEntity.GetHandle()) } });
                });

            InCommands.Add(CommandSpec("camera.trace.start",
                "Records every frame of the playing world from the next one: the frame's time, the view, and where "
                "{target} (an entity's name, optional) is drawn; {name} names the files. {heightPx}: the view's height "
                "the pixels are measured in (the window's by default).")
                    .Param("name", ECommandParamType::String, "names the trace files")
                    .Param("target", ECommandParamType::String, "an entity to follow on screen").Optional()
                    .Param("heightPx", ECommandParamType::Float, "the view's height in pixels").Optional(),
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

            InCommands.Add(CommandSpec("camera.trace.stop",
                "Ends the trace, writes <name>.camera.json and <name>.camera.csv in the working folder and answers "
                "{name, frames, files, summary}: the target's largest move on screen (targetDrift), the largest change "
                "of the view's move (motionJitter), largestMove (pixels), timeReversals, stillMoves, zoomReversals, "
                "cameraSwitches, switchJump (how much a switch changed the view's move: a cut's jump, near 0 for a "
                "blend), cuts, and the most and fewest steps a frame took (mostSteps, fewestSteps, stepsSpread). "
                "{limits}, like {\"targetDrift\": 0.01}, fails the request past any of them.")
                    .Param("limits", ECommandParamType::Any, "a JSON object of limits").Optional(),
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

            InCommands.Add(CommandSpec("camera.trace.compare",
                "How far the stopped trace {trace} is from where the stopped trace {reference} says its camera was at "
                "each of its game times (the reference's frame then, or its two frames around it blended by time): "
                "{shared, position, size (world units), rotation (degrees), atTime}. The reference is a steady pacing "
                "at the fixed rate or a multiple, so one level played at any rate must match it; a view curving "
                "within a step (a blend, a shake) needs a reference drawing within the step (720 places 60, 144 and "
                "240 exactly). {limits}, like {\"position\": 0.001}, fails past them.")
                    .Param("trace", ECommandParamType::String, "the trace to measure")
                    .Param("reference", ECommandParamType::String, "the trace to measure against")
                    .Param("limits", ECommandParamType::Any, "a JSON object of limits").Optional(),
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
                        { "position", lGap.Position }, { "size", lGap.Size }, { "rotation", lGap.Rotation },
                        { "atTime", lGap.AtTime } };

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
        void RegisterConfig(ICommandRegistry& InCommands)
        {
            InCommands.Add(CommandSpec("config.get",
                "A config's values: {config}, its file's name (\"Engine\", \"Renderer\"...). Answers {config, values} "
                "what the engine reads, {project} the file's values and {player} the player's settings over them.")
                    .Param("config", ECommandParamType::String, "the config file's name: Engine, Renderer..."),
                [](const nlohmann::json& InParams)
                {
                    const OpaaxString lName = InParams.value("config", OpaaxString());
                    OpaaxString       lError;
                    const Config*     lConfig = FindConfig(lName, lError);
                    if (lConfig == nullptr)
                    {
                        return AutomationResult::Fail(lError);
                    }

                    return AutomationResult::Ok(DescribeConfig(lName, *lConfig));
                });

            InCommands.Add(CommandSpec("config.set",
                "Merges {values}, shaped like the config's file ({\"Time\": {\"FixedRate\": 30}}), into {config}'s "
                "project values for this run, without saving; applied at once, as the Config panel does. A value the "
                "config does not have, or of the wrong type, is refused; one the player set still wins. Answers as "
                "config.get after.")
                    .Param("config", ECommandParamType::String, "the config file's name")
                    .Param("values", ECommandParamType::Any, "a JSON object shaped like the file").Cheat(),
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

                    return AutomationResult::Ok(DescribeConfig(lName, *lConfig));
                });
        }

        // =========================================================================
        // Expectations: a failed one fails the request, so a script's exit code fails a test run
        // =========================================================================
        void RegisterExpectations(ICommandRegistry& InCommands, IEngine& InEngine)
        {
            InCommands.Add(CommandSpec("expect.value",
                "Fails unless {entity}'s value at {path} (\"Transform/Position/x\") meets: equals, near (with "
                "tolerance), greater, less or between [min, max].")
                    .Param("entity", ECommandParamType::String, "the entity's id or name")
                    .Param("path", ECommandParamType::String, "Component/Field/sub")
                    .Param("equals", ECommandParamType::Any, "the value it must be").Optional()
                    .Param("near", ECommandParamType::Float, "the value it must be near").Optional()
                    .Param("tolerance", ECommandParamType::Float, "how near (0.01)").Optional()
                    .Param("greater", ECommandParamType::Float, "it must be above").Optional()
                    .Param("less", ECommandParamType::Float, "it must be below").Optional()
                    .Param("between", ECommandParamType::Any, "[min, max]").Optional(),
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

            InCommands.Add(CommandSpec("expect.count",
                "Fails unless the number of entities ({name}, {component} filter them) meets: equals, greater, "
                "less or between [min, max].")
                    .Param("name", ECommandParamType::String, "count the entities of this name").Optional()
                    .Param("component", ECommandParamType::String, "count the entities with this component").Optional()
                    .Param("equals", ECommandParamType::Int, "the count it must be").Optional()
                    .Param("greater", ECommandParamType::Int, "it must be above").Optional()
                    .Param("less", ECommandParamType::Int, "it must be below").Optional()
                    .Param("between", ECommandParamType::Any, "[min, max]").Optional(),
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

            InCommands.Add(CommandSpec("expect.entity",
                "Fails unless an entity named {entity} (or with that id) exists; with {exists: false}, unless none does.")
                    .Param("entity", ECommandParamType::String, "the entity's id or name")
                    .Param("exists", ECommandParamType::Bool, "false: it must not exist").Optional(),
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

        // =========================================================================
        // Component patches
        // =========================================================================

        /**
         * InType's entry, when InValue (null when none is given) is a patch it takes: an object of
         * fields. Null otherwise, with OutError saying what to do.
         */
        const IComponentEntry* FindPatchTarget(const ComponentRegistry& InTypes, const OpaaxString& InType,
                                               const nlohmann::json* InValue, OpaaxString& OutError)
        {
            const IComponentEntry* lEntry = InTypes.FindByName(OpaaxStringID(InType));
            if (lEntry == nullptr)
            {
                OutError = "unknown component type '" + InType + "': check its name (entity.get lists an entity's), "
                           "or rebuild if the type was added since this build";
                return nullptr;
            }
            if (InValue != nullptr && !InValue->is_object())
            {
                OutError = "the values of '" + InType + "' are an object of the fields to change, not "
                           + ToJsonText(*InValue);
                return nullptr;
            }
            return lEntry;
        }

        /** One component of an entity as it was before a patch: its values, or that it was missing. */
        struct ComponentSnapshot
        {
            const IComponentEntry* Entry = nullptr;
            bool                   bHad  = false;
            nlohmann::json         Values;
        };

        ComponentSnapshot TakeSnapshot(const IComponentEntry& InEntry, const EntityRegistry& InRegistry,
                                       const EntityID InEntity)
        {
            ComponentSnapshot lSnapshot{ &InEntry, InEntry.Has(InRegistry, InEntity), nlohmann::json::object() };
            if (lSnapshot.bHad)
            {
                lSnapshot.Values = InEntry.Save(InRegistry, InEntity);
            }
            if (lSnapshot.Values.is_null())
            {
                lSnapshot.Values = nlohmann::json::object();
            }
            return lSnapshot;
        }

        /** Puts the component back as InSnapshot holds it: its values, or removed when it was missing. */
        void RestoreSnapshot(const ComponentSnapshot& InSnapshot, EntityRegistry& InRegistry, const EntityID InEntity)
        {
            if (InSnapshot.bHad)
            {
                InSnapshot.Entry->Load(InRegistry, InEntity, InSnapshot.Values);
            }
            else
            {
                InSnapshot.Entry->Remove(InRegistry, InEntity);
            }
        }
    }

    void Register(ICommandRegistry& InCommands, IEngine& InEngine, IAutomationHost& InHost)
    {
        RegisterApp(InCommands, InHost);
        RegisterInput(InCommands, InEngine);
        InputAutomation::Register(InCommands, InEngine);
        RegisterUI(InCommands, InEngine);
        RegisterWorld(InCommands, InEngine);
        RegisterLevels(InCommands, InEngine);
        RegisterTime(InCommands, InEngine);
        RegisterWindow(InCommands, InEngine);
        RegisterConfig(InCommands);
        RegisterCamera(InCommands, InEngine);
        RegisterExpectations(InCommands, InEngine);
        SaveAutomation::Register(InCommands, InEngine);
        OnlineAutomation::Register(InCommands);
        SettingsAutomation::Register(InCommands);
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

    bool CheckComponents(const ComponentRegistry& InTypes, const nlohmann::json& InComponents, OpaaxString& OutError)
    {
        if (!InComponents.is_object())
        {
            OutError = "\"components\" is an object: {type: values}";
            return false;
        }

        for (const auto& [lType, lValue] : InComponents.items())
        {
            if (FindPatchTarget(InTypes, FromJsonString(lType), &lValue, OutError) == nullptr)
            {
                return false;
            }
        }
        return true;
    }

    bool PatchComponent(Entity InEntity, const ComponentRegistry& InTypes, const nlohmann::json& InParams,
                        OpaaxString& OutError)
    {
        const OpaaxString      lType  = InParams.value("type", OpaaxString());
        const auto             lValue = InParams.find("value");
        const bool             bValue = lValue != InParams.end();
        const IComponentEntry* lEntry = FindPatchTarget(InTypes, lType, bValue ? &*lValue : nullptr, OutError);
        if (lEntry == nullptr)
        {
            return false;
        }

        EntityRegistry&         lRegistry = InEntity.GetWorld()->GetRegistry();
        const EntityID          lHandle   = InEntity.GetHandle();
        const ComponentSnapshot lBefore   = TakeSnapshot(*lEntry, lRegistry, lHandle);

        nlohmann::json lData = lBefore.Values;
        if (bValue)
        {
            lData.merge_patch(*lValue);
        }

        try
        {
            lEntry->Load(lRegistry, lHandle, lData);
        }
        catch (const nlohmann::json::exception& lError)
        {
            // A field of the wrong type stops the load part way: the component goes back to what it was.
            RestoreSnapshot(lBefore, lRegistry, lHandle);
            OutError = OpaaxString::Format("'{}' did not take these values ({}): give each field the type "
                                           "entity.get shows for it", lType, lError.what());
            return false;
        }
        return true;
    }

    bool PatchComponents(Entity InEntity, const ComponentRegistry& InTypes, const nlohmann::json& InComponents,
                         OpaaxString& OutError)
    {
        if (!CheckComponents(InTypes, InComponents, OutError))
        {
            return false;
        }

        // Each one as it was, taken before it is patched: a later failure puts the earlier ones back.
        EntityRegistry&              lRegistry = InEntity.GetWorld()->GetRegistry();
        const EntityID               lHandle   = InEntity.GetHandle();
        TDynArray<ComponentSnapshot> lPatched;
        for (const auto& [lType, lValue] : InComponents.items())
        {
            const IComponentEntry&  lEntry    = *InTypes.FindByName(OpaaxStringID(FromJsonString(lType)));
            const ComponentSnapshot lSnapshot = TakeSnapshot(lEntry, lRegistry, lHandle);
            if (!PatchComponent(InEntity, InTypes, nlohmann::json{ { "type", lType }, { "value", lValue } }, OutError))
            {
                for (const ComponentSnapshot& lDone : lPatched)
                {
                    RestoreSnapshot(lDone, lRegistry, lHandle);
                }
                return false;
            }
            lPatched.push_back(lSnapshot);
        }
        return true;
    }
}
