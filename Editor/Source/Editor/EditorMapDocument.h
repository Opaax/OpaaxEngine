#pragma once

#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"
#include "Core/String/OpaaxStringID.hpp"
#include "Core/Log/Logger.h"
#include "World/Entity/EntityTypes.h"   // MapId

namespace Opaax
{
    struct MapData;
}

namespace Opaax::Editor
{
    inline constexpr LogCategory LogEditorMapDocument{"EditorMapDocument"};

    // =============================================================================
    // EditorMapDocument — which of the level's maps is focused. Only a cursor: no baseline, no save
    //   (EditorLevelDocument owns those). The focus decides which map the single-map commands act on
    //   (Save Map, Save Map As, Remove Open Map, Set Open Map Persistent).
    // =============================================================================
    class EditorMapDocument
    {
        // =============================================================================
        // Ctor
        // =============================================================================
    public:
        EditorMapDocument() = default;

        EditorMapDocument(const EditorMapDocument&)            = delete;
        EditorMapDocument& operator=(const EditorMapDocument&) = delete;

        // =============================================================================
        // Functions
        // =============================================================================
    public:
        /**
         * Focuses a map. Reads the file only to find which map its entities claim; falls back to the
         * file name ("Main.opaaxmap" -> "Main"), also used for a map that does not exist yet.
         */
        void Focus(const OpaaxString& InAbsPath);

        /** Focuses nothing. */
        void Clear();

        // =============================================================================
        // Get
        // =============================================================================
    public:
        bool               HasMap()   const noexcept { return !m_AbsPath.IsEmpty(); }
        const OpaaxString& AbsPath()  const noexcept { return m_AbsPath; }
        MapId              GetMapId() const noexcept { return m_MapId; }

        /** The file name, for the menu bar ("Main.opaaxmap"). Empty when none is focused. */
        OpaaxString FileName() const;

        // =============================================================================
        // Members
        // =============================================================================
    private:
        /**
         * The MapId InData's entities claim, or InAbsPath's file name when none does.
         */
        static MapId DeriveMapId(const OpaaxString& InAbsPath, const MapData& InData);

        OpaaxString m_AbsPath;
        MapId       m_MapId;
    };
}
