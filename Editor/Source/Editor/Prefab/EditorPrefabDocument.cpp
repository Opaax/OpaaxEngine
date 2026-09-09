#include "Editor/Prefab/EditorPrefabDocument.h"

#include "Editor/EditorContext.h"
#include "Editor/Operation/ResourceOperations.h"

#include "Application/Services/IEngine.h"
#include "Engine/Registries/EngineRegistries.h"

#include "World/Components/ComponentRegistry.h"
#include "World/Prefab/PrefabFactory.h"
#include "World/Prefab/PrefabFile.h"
#include "World/Prefab/PrefabJson.h"
#include "World/Prefab/PrefabResource.hpp"
#include "World/Serialization/MapFactory.h"
#include "World/Serialization/MapSerializer.h"
#include "World/World.h"
#include "World/WorldManager.h"

namespace Opaax::Editor
{
    namespace
    {
        // The world's entities as a prefab — the same transform Create Prefab from Selection uses,
        // so what the panel writes and what the Hierarchy writes cannot mean different things.
        PrefabData CaptureAsPrefab(World& InWorld, const ComponentRegistry& InRegistry)
        {
            return PrefabFactory::BuildPrefab(MapSerializer::CaptureWorld(InWorld, InRegistry),
                                              InRegistry);
        }
    }

    bool EditorPrefabDocument::Open(EditorContext& InContext, const OpaaxString& InAbsPath)
    {
        PrefabData lData;
        if (!PrefabFile::Load(InAbsPath, lData))
        {
            // The previous document stays open — a failed read must not half-replace it (**MP3**).
            return false;
        }

        if (m_World == nullptr)
        {
            // Created ONCE and kept for the session; see the header for why it is never destroyed.
            m_World = InContext.Worlds.CreateWorld(OpaaxString("Prefab Edit"), EWorldMode::Edit);

            if (m_World == nullptr)
            {
                OPAAX_LOG(LogEditorPrefabDocument, Error, "No world to edit prefabs in");
                return false;
            }
        }

        m_World->Clear();

        const ComponentRegistry& lRegistry = InContext.Engine.GetRegistries().Components();

        // The prefab's entities carry no map (**WM2**) and none is stamped: this world holds the
        // TEMPLATES, not a placement, so their guids are the file's own.
        MapData lAsMap;
        lAsMap.Entities = lData.Entities;

        const Uint64 lCreated = MapFactory::Instantiate(lAsMap, *m_World, lRegistry);

        m_AbsPath  = InAbsPath;
        m_Baseline = PrefabJson::Serialize(CaptureAsPrefab(*m_World, lRegistry));

        OPAAX_LOG(LogEditorPrefabDocument, Info, "Editing prefab '{}' — {} entity(ies)",
                  InAbsPath.CStr(), lCreated);

        return true;
    }

    bool EditorPrefabDocument::Save(EditorContext& InContext)
    {
        if (!IsOpen() || m_World == nullptr) { return false; }

        const ComponentRegistry& lRegistry = InContext.Engine.GetRegistries().Components();
        const PrefabData         lData     = CaptureAsPrefab(*m_World, lRegistry);

        if (!PrefabFile::Save(m_AbsPath, lData))
        {
            // Baseline untouched, so the document keeps reporting unsaved work rather than claiming
            // a file it never wrote — SaveMap's rule.
            return false;
        }

        m_Baseline = PrefabJson::Serialize(lData);

        // THE POINT OF EDITING A PREFAB (P4): reload the resource and re-apply it to every placement
        // in the level, each keeping its own overrides.
        ResourceOps::SavedToDisk<PrefabResource>(InContext, m_AbsPath);

        return true;
    }

    void EditorPrefabDocument::Close()
    {
        m_AbsPath  = OpaaxString();
        m_Baseline = OpaaxString();

        if (m_World != nullptr) { m_World->Clear(); }
    }

    bool EditorPrefabDocument::IsDirty(EditorContext& InContext) const
    {
        if (!IsOpen() || m_World == nullptr) { return false; }

        return PrefabJson::Serialize(
                   CaptureAsPrefab(*m_World, InContext.Engine.GetRegistries().Components()))
               != m_Baseline;
    }
}
