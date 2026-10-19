#include "Editor/EditorMapDocument.h"

#include "Core/IO/FileIO.h"
#include "Core/String/OpaaxPathString.h"
#include "World/Serialization/MapData.h"
#include "World/Serialization/MapFile.h"   // StemId
#include "World/Serialization/MapJson.h"

namespace Opaax::Editor
{
    MapId EditorMapDocument::DeriveMapId(const OpaaxString& InAbsPath, const MapData& InData)
    {
        if (InData.Id.IsValid())
        {
            return InData.Id;   // the map's own name (declared, or claimed by its entities)
        }

        // The file does not exist yet (Save As target): same naming rule as MapFile::Load.
        return MapFile::StemId(InAbsPath);
    }

    void EditorMapDocument::Focus(const OpaaxString& InAbsPath)
    {
        m_AbsPath = InAbsPath;

        // MapJson, not MapFile::Load: this runs on every focus change and MapFile logs each read.
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

        return PathString::FileName(m_AbsPath).ToString();
    }
}
