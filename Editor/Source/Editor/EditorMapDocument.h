#pragma once

#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"
#include "Core/String/OpaaxStringID.hpp"
#include "Application/Services/ILogger.h"
#include "World/Entity/EntityTypes.h"   // MapId

namespace Opaax
{
    struct MapData;
}

namespace Opaax::Editor
{
    inline constexpr LogCategory LogEditorMapDocument{"EditorMapDocument"};

    // =============================================================================
    // EditorMapDocument — WHICH of the level's maps is FOCUSED. A cursor, and only that.
    //
    //   It owns no baseline and cannot save. Every mounted map's unsaved state lives in
    //   EditorLevelDocument, one owner, because a second baseline for the same map would rebase on
    //   Save Map while the other did not and the dirty marker would start lying ([[L30]]).
    //
    //   What the focus decides is which map the SINGLE-map commands act on — Save Map, Save Map
    //   As, Remove Open Map, Set Open Map Persistent — and which group the Hierarchy opens. It
    //   decides nothing about what a Save Level writes: that is every map (**MP9**).
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
         * Point the cursor at a map. Reads the file ONLY to ask its entities which map they claim.
         *
         * The MapId comes from the FILE's entities, not from its name — they are the authority on
         * which map they belong to (**WM2**), and a file renamed on disk does not re-stamp what is
         * inside it. A file with nobody to ask falls back to the stem ("Main.opaaxmap" -> "Main"),
         * which is also the right default for a map that does not exist yet.
         */
        void Focus(const OpaaxString& InAbsPath);

        /** Point at nothing — no map is focused. */
        void Clear();

        // =============================================================================
        // Get
        // =============================================================================
    public:
        bool               HasMap()   const noexcept { return !m_AbsPath.IsEmpty(); }
        const OpaaxString& AbsPath()  const noexcept { return m_AbsPath; }
        MapId              GetMapId() const noexcept { return m_MapId; }

        /** Just the file name, for the menu bar — "Main.opaaxmap". Empty when none is focused. */
        OpaaxString FileName() const;

        // =============================================================================
        // Members
        // =============================================================================
    private:
        /**
         * The MapId InData's entities claim, or InAbsPath's file stem when none of them does.
         *
         * Takes the MAP, not a world: a world holds several maps (**WM1a**), so its first valid
         * OwnerMap answers a different question than the one being asked.
         */
        static MapId DeriveMapId(const OpaaxString& InAbsPath, const MapData& InData);

        OpaaxString m_AbsPath;
        MapId       m_MapId;
    };
}
