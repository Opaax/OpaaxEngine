#include "Automation/EngineAutomationCommands.h"

#include <algorithm>
#include <filesystem>

#include "Application/OpaaxApplication.h"
#include "Application/Services/IEngine.h"
#include "Application/Services/IPaths.h"
#include "Engine/Registries/EngineRegistries.h"
#include "Input/InputKeyNames.h"
#include "Input/InputManager.h"
#include "World/Components/ComponentRegistry.h"
#include "World/Entity/Entity.h"
#include "World/Entity/EntityMeta.h"
#include "World/World.h"
#include "World/WorldManager.h"

namespace Opaax::EngineAutomation
{
    namespace
    {
        /** The frames a tapped key stays down, so a fixed-step game surely sees it. */
        constexpr Uint32 DEFAULT_TAP_FRAMES = 2;

        std::string Text(const OpaaxString& InText)
        {
            return std::string(InText.CStr());
        }

        /** The key or mouse button InName names ("Space", "A", "Mouse_Left"...). */
        bool ParseKey(const std::string& InName, EKeyCode& OutKey)
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
            return std::string(ToString(InKey)).rfind("Mouse_", 0) == 0;
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

        World* ActiveWorld(IEngine& InEngine, std::string& OutError)
        {
            World* lWorld = InEngine.GetWorldManager().GetActiveWorld();
            if (lWorld == nullptr)
            {
                OutError = "no active world";
            }
            return lWorld;
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
                        { "workspace", Text(lPaths.WorkspaceRoot()) },
                        { "project",   Text(lPaths.ProjectRoot()) },
                        { "assets",    Text(lPaths.AssetsDir()) },
                        { "save",      Text(lPaths.SaveDir()) } });
                });

            InRunner.Register("app.quit", "Closes the app at the end of this frame.",
                [&InHost](const nlohmann::json&)
                {
                    InHost.RequestQuit();
                    return AutomationResult::Ok();
                });

            InRunner.Register("screenshot",
                "Saves this frame as a PNG at {path} (relative paths are from the working folder); the next "
                "request runs once it is written.",
                [&InHost](const nlohmann::json& InParams)
                {
                    const std::string lPath = InParams.value("path", std::string());
                    if (lPath.empty())
                    {
                        return AutomationResult::Fail("\"path\" names the PNG to write");
                    }

                    const std::string lAbsolute = std::filesystem::absolute(lPath).generic_string();
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
                    const std::string lName = InParams.value("key", std::string());
                    if (!ParseKey(lName, lKey))
                    {
                        return AutomationResult::Fail("unknown key '" + lName + "'");
                    }

                    InputManager&     lInput  = InEngine.GetInput();
                    const std::string lAction = InParams.value("action", std::string("tap"));
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
                    lResult.WaitFrames = std::max(InParams.value("frames", DEFAULT_TAP_FRAMES), 1u);
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
        // World
        // =========================================================================
        void RegisterWorld(AutomationRunner& InRunner, IEngine& InEngine)
        {
            InRunner.Register("world.info", "The active world: {name, mode, paused, entities, camera}.",
                [&InEngine](const nlohmann::json&)
                {
                    std::string  lError;
                    World* const lWorld = ActiveWorld(InEngine, lError);
                    if (lWorld == nullptr)
                    {
                        return AutomationResult::Fail(lError);
                    }

                    const CameraView& lView = lWorld->GetCameraView();
                    return AutomationResult::Ok(nlohmann::json{
                        { "name",     Text(lWorld->GetName()) },
                        { "mode",     ToString(lWorld->GetMode()) },
                        { "paused",   InEngine.GetWorldManager().IsPaused() },
                        { "entities", lWorld->GetEntityCount() },
                        { "camera",   { { "x", lView.Position.x }, { "y", lView.Position.y }, { "size", lView.OrthoSize } } } });
                });

            InRunner.Register("entity.list",
                "The active world's entities: [{id, name, parent, map, components}]. Filters: {name}, {component}.",
                [&InEngine](const nlohmann::json& InParams)
                {
                    std::string  lError;
                    World* const lWorld = ActiveWorld(InEngine, lError);
                    if (lWorld == nullptr)
                    {
                        return AutomationResult::Fail(lError);
                    }

                    const ComponentRegistry& lTypes     = InEngine.GetRegistries().Components();
                    const std::string        lName      = InParams.value("name", std::string());
                    const std::string        lComponent = InParams.value("component", std::string());

                    nlohmann::json lList = nlohmann::json::array();
                    lWorld->Each<EntityMeta>([&](const EntityID InId, const EntityMeta& InMeta)
                    {
                        if (!lName.empty() && lName != InMeta.Name.CStr())
                        {
                            return;
                        }

                        nlohmann::json lEntity = DescribeEntity(Entity{ InId, lWorld }, lTypes, false);
                        if (!lComponent.empty()
                            && std::find(lEntity["components"].begin(), lEntity["components"].end(), lComponent)
                                   == lEntity["components"].end())
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
                    std::string  lError;
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
                    std::string  lError;
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
    }

    void Register(AutomationRunner& InRunner, IEngine& InEngine, IAutomationHost& InHost)
    {
        RegisterApp(InRunner, InHost);
        RegisterInput(InRunner, InEngine);
        RegisterWorld(InRunner, InEngine);
    }

    Entity FindEntity(World& InWorld, const nlohmann::json& InParams, std::string& OutError)
    {
        const auto lRef = InParams.find("entity");
        if (lRef == InParams.end() || !lRef->is_string() || lRef->get<std::string>().empty())
        {
            OutError = "\"entity\" names an entity: its id or its name";
            return Entity{};
        }

        const std::string lText = lRef->get<std::string>();

        Guid lId;
        if (Guid::FromString(OpaaxString(lText.c_str()), lId))
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
            if (lText == InMeta.Name.CStr())
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
            OutError = std::to_string(lMatches) + " entities are named '" + lText + "': name one by its id";
            return Entity{};
        }
        return lFound;
    }

    nlohmann::json DescribeEntity(Entity InEntity, const ComponentRegistry& InTypes, const bool bInWithValues)
    {
        nlohmann::json lJson = nlohmann::json::object();
        if (const EntityMeta* lMeta = InEntity.TryGet<EntityMeta>())
        {
            lJson["id"]     = Text(lMeta->Id.ToString());
            lJson["name"]   = Text(lMeta->Name);
            lJson["parent"] = lMeta->Parent.IsValid() ? nlohmann::json(Text(lMeta->Parent.ToString())) : nlohmann::json();
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
                        std::string& OutError)
    {
        const std::string      lType  = InParams.value("type", std::string());
        const IComponentEntry* lEntry = InTypes.FindByName(OpaaxStringID(lType.c_str()));
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
