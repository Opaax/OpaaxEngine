#include "Editor/EditorMapDocument.h"

#include <string>

#include "Core/IO/FileIO.h"          // the adopt-time round-trip stability check
#include "World/ComponentRegistry.h"
#include "World/Entity/EntityMeta.h"
#include "World/Serialization/MapFactory.h"
#include "World/Serialization/MapFile.h"
#include "World/Serialization/MapJson.h"
#include "World/Serialization/MapSerializer.h"
#include "World/World.h"

namespace Opaax::Editor
{
    OpaaxString EditorMapDocument::Serialize(const World& InWorld, const ComponentRegistry& InRegistry) const
    {
        // FILTERED by the document's MapId — "save this map", not "save the world". A
        // runtime-spawned entity carries an invalid OwnerMap and so can never match (WM2), which
        // is what keeps bullets and VFX out of an authored map without a special case.
        return MapJson::Serialize(MapSerializer::Capture(InWorld, InRegistry, m_MapId));
    }

    MapId EditorMapDocument::DeriveMapId(const OpaaxString& InAbsPath, const World& InWorld)
    {
        // The entities are the authority on which map they belong to (WM2). Renaming a file on
        // disk does not re-stamp what is inside it, so the world is asked first.
        MapId lFound;
        const_cast<World&>(InWorld).Each<EntityMeta>(
            [&lFound](auto, const EntityMeta& InMeta)
            {
                if (!lFound.IsValid() && InMeta.OwnerMap.IsValid()) { lFound = InMeta.OwnerMap; }
            });

        if (lFound.IsValid())
        {
            return lFound;
        }

        // An empty world has nobody to ask. The file's stem is the fallback, which is also the
        // right default for authoring a map that does not exist yet.
        const std::string lPath  = std::string(InAbsPath.CStr());
        const size_t      lSlash = lPath.find_last_of("/\\");
        const size_t      lStart = (lSlash == std::string::npos) ? 0 : lSlash + 1;
        const size_t      lDot   = lPath.find_last_of('.');
        const size_t      lEnd   = (lDot == std::string::npos || lDot < lStart) ? lPath.size() : lDot;
        const std::string lStem  = lPath.substr(lStart, lEnd - lStart);

        return lStem.empty() ? MapId() : MapId(lStem);
    }

    void EditorMapDocument::AdoptExisting(const OpaaxString& InAbsPath,
                                          const World& InWorld, const ComponentRegistry& InRegistry)
    {
        m_AbsPath  = InAbsPath;
        m_MapId    = DeriveMapId(InAbsPath, InWorld);

        // The baseline comes from the WORLD, not from re-reading the file. They agree at this
        // point (the engine just built the world from it), and taking it from the world is what
        // makes a freshly-opened editor clean even if the file's formatting differs from what
        // this build would write — otherwise every launch would open dirty for a reason no author
        // could act on.
        m_Baseline = Serialize(InWorld, InRegistry);

        OPAAX_LOG(LogEditorMapDocument, Info, "Editing map '{}' (map id '{}')",
                  m_AbsPath.CStr(), m_MapId.IsValid() ? m_MapId.ToString().CStr() : "(none)");

        // ROUND-TRIP STABILITY CHECK, and it earns its file read.
        //
        // The whole milestone rests on world -> file -> world -> file being a fixed point. If the
        // text this build produces from the loaded world differs from the file that world came
        // from, then every Save rewrites the map with churn nobody asked for — a git diff full of
        // reordered or reformatted entities around the one value that actually changed. That
        // failure is completely silent otherwise: the map still loads, the world still looks
        // right, and only the diff shows it.
        //
        // Info when it holds, Warn when it does not, so the answer is in the log either way
        // rather than only when something breaks.
        const OpaaxString lOnDisk = FileIO::ReadAllText(m_AbsPath);
        if (lOnDisk.IsEmpty())
        {
            return;   // no file yet (a brand-new map) — nothing to compare against
        }

        if (lOnDisk == m_Baseline)
        {
            OPAAX_LOG(LogEditorMapDocument, Info,
                      "Round trip is stable — the world re-serializes to exactly the file it came from");
        }
        else
        {
            OPAAX_LOG(LogEditorMapDocument, Warn,
                      "The world re-serializes DIFFERENTLY from '{}' — the next Save will rewrite it "
                      "(formatting churn, or a component the registry no longer knows)", m_AbsPath.CStr());
        }
    }

    bool EditorMapDocument::Save(const World& InWorld, const ComponentRegistry& InRegistry)
    {
        if (!HasMap())
        {
            OPAAX_LOG(LogEditorMapDocument, Warn, "Save: no map is open — use Save As");
            return false;
        }

        // ONE capture, used for both the file and the new baseline — walking the world twice for
        // a single Save would be the obvious version of this and pointlessly so.
        const MapData     lData = MapSerializer::Capture(InWorld, InRegistry, m_MapId);
        const OpaaxString lText = MapJson::Serialize(lData);

        if (!MapFile::Save(m_AbsPath, lData))
        {
            // Baseline deliberately UNTOUCHED: the document must keep reporting unsaved work
            // rather than claim to be clean against a file that was never written.
            OPAAX_LOG(LogEditorMapDocument, Error, "Save FAILED for '{}' — document still has unsaved changes",
                      m_AbsPath.CStr());
            return false;
        }

        // The world now IS the file, so rebase from the text we just serialized rather than
        // capturing a second time.
        m_Baseline = lText;

        OPAAX_LOG(LogEditorMapDocument, Info, "Saved '{}'", m_AbsPath.CStr());
        return true;
    }

    bool EditorMapDocument::SaveAs(const OpaaxString& InAbsPath, const World& InWorld,
                                   const ComponentRegistry& InRegistry)
    {
        if (InAbsPath.IsEmpty())
        {
            return false;   // the dialog was cancelled
        }

        const OpaaxString lPrevious = m_AbsPath;

        m_AbsPath = InAbsPath;
        if (!Save(InWorld, InRegistry))
        {
            // Failing to write the NEW path must not leave the document pointing at it — the next
            // plain Save would then silently target a file that does not exist.
            m_AbsPath = lPrevious;
            return false;
        }

        return true;
    }

    bool EditorMapDocument::Open(const OpaaxString& InAbsPath, World& InWorld, const ComponentRegistry& InRegistry)
    {
        MapData lData;

        // Read BEFORE clearing: a file that cannot be read must not cost the author the world
        // they already had. MapFile::Load leaves lData untouched on failure.
        if (!MapFile::Load(InAbsPath, lData))
        {
            OPAAX_LOG(LogEditorMapDocument, Error, "Open FAILED for '{}' — the current map is unchanged",
                      InAbsPath.CStr());
            return false;
        }

        // Replace, not merge. The World OBJECT survives — only its entities go — so its identity,
        // its subsystems and the viewport's render target are all undisturbed.
        InWorld.Clear();
        MapFactory::Instantiate(lData, InWorld, InRegistry);

        AdoptExisting(InAbsPath, InWorld, InRegistry);
        return true;
    }

    bool EditorMapDocument::IsDirty(const World& InWorld, const ComponentRegistry& InRegistry) const
    {
        if (!HasMap())
        {
            return false;   // nothing to be dirty against
        }

        return Serialize(InWorld, InRegistry) != m_Baseline;
    }

    OpaaxString EditorMapDocument::FileName() const
    {
        if (!HasMap()) { return OpaaxString(); }

        const std::string lPath(m_AbsPath.CStr());
        const size_t      lSlash = lPath.find_last_of("/\\");

        return OpaaxString(lSlash == std::string::npos ? lPath.c_str() : lPath.c_str() + lSlash + 1);
    }
}
