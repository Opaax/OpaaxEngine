#include "Editor/EditorMapDocument.h"

#include <string>

#include "Core/IO/FileIO.h"
#include "World/Serialization/MapData.h"
#include "World/Serialization/MapFile.h"   // StemId — one naming rule, shared with Load
#include "World/Serialization/MapJson.h"

namespace Opaax::Editor
{
    MapId EditorMapDocument::DeriveMapId(const OpaaxString& InAbsPath, const MapData& InData)
    {
        if (InData.Id.IsValid())
        {
            return InData.Id;   // the map's own name — declared, or what its entities claim (MP10)
        }

        // The file does not exist yet (a Save As target), so there was nothing to declare it. Same
        // rule MapFile::Load applies to a file that does, from the same function — a naming rule
        // with two copies is a naming rule with two answers.
        return MapFile::StemId(InAbsPath);
    }

    void EditorMapDocument::Focus(const OpaaxString& InAbsPath)
    {
        m_AbsPath = InAbsPath;

        // Read through MapJson rather than MapFile::Load: this happens on every focus change and
        // MapFile logs an Info per read, which would turn clicking between maps into log noise.
        const OpaaxString lOnDisk = FileIO::ReadAllText(m_AbsPath);

        MapData lData;
        if (!lOnDisk.IsEmpty()) { MapJson::Deserialize(lOnDisk, lData); }

        m_MapId = DeriveMapId(m_AbsPath, lData);

        OPAAX_LOG(LogEditorMapDocument, Info, "Focused map '{}' (map id '{}')",
                  m_AbsPath.CStr(), m_MapId.IsValid() ? m_MapId.CStr() : "(none)");
    }

    void EditorMapDocument::Clear()
    {
        m_AbsPath = OpaaxString();
        m_MapId   = MapId();
    }

    OpaaxString EditorMapDocument::FileName() const
    {
        if (!HasMap()) { return OpaaxString(); }

        const std::string lPath(m_AbsPath.CStr());
        const size_t      lSlash = lPath.find_last_of("/\\");

        return OpaaxString(lSlash == std::string::npos ? lPath.c_str() : lPath.c_str() + lSlash + 1);
    }
}
