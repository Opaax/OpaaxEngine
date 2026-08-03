#pragma once

#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"
#include "Core/String/OpaaxStringID.hpp"
#include "Application/Services/ILogger.h"
#include "World/Entity/EntityTypes.h"   // MapId

namespace Opaax
{
    class World;
    class ComponentRegistry;
}

namespace Opaax::Editor
{
    inline constexpr LogCategory LogEditorMapDocument{"EditorMapDocument"};

    // =============================================================================
    // EditorMapDocument — WHICH map the editor has open, and whether it has been changed.
    //
    //   The editor's half of M5's author loop. It owns three things: the absolute path of the
    //   open `.opaaxmap`, the MapId the world's entities are stamped with, and the json text as
    //   it was last written or read — the BASELINE.
    //
    //   DIRTY IS DERIVED, NOT TRACKED, and that is the design rather than a shortcut:
    //     - There is nothing to hook. Every edit in this editor goes through a registered drawer
    //       (DrawerRegistry), whose contract is `bool(Entity&)` meaning "was it DRAWN" — not "was
    //       it CHANGED". Tracking would mean changing that contract and every drawer with it,
    //       including a game's.
    //     - A tracked flag gets the interesting case wrong. Drag a quad away and back, and a flag
    //       says dirty while the file on disk is already correct. Comparing against the baseline
    //       says clean, which is the truth.
    //     - It cannot drift. A derived answer has no second copy of the state to fall out of sync
    //       with the world it describes.
    //   The cost is a capture + serialize per check, so it is checked WHEN ASKED (the menu opens,
    //   Play starts, the frame draws the title) and never in a loop. MapJson sorting entities by
    //   Guid is what makes the comparison independent of entt's storage order — without it, an
    //   entity destroyed anywhere would reshuffle the text and report a world nobody edited.
    //
    //   Owned by EditorService, referenced from EditorContext, exactly as Selection / PIE /
    //   InputRoute are — so a panel or a menu command reaches it by ctor and never the locator (D3).
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
         * Point the document at a map WITHOUT reading or writing anything.
         *
         * Used at startup, where the world was already filled by the engine's level load: the
         * document adopts what is on screen and takes its baseline from the current world, so a
         * freshly-opened editor is CLEAN rather than dirty-by-default.
         *
         * The MapId is taken from the WORLD's entities, not from the file name — they are the
         * authority on which map they belong to (WM2), and a file renamed on disk does not
         * re-stamp what is inside it. An empty world has nobody to ask, so the file's stem is the
         * fallback ("Main.opaaxmap" -> "Main"), which is also the right default for authoring a
         * brand-new map.
         *
         * @param InAbsPath Absolute path of the `.opaaxmap` this world came from.
         */
        void AdoptExisting(const OpaaxString& InAbsPath, const World& InWorld, const ComponentRegistry& InRegistry);

        /**
         * Write the map, then rebase: after a successful save the world IS the file, so the
         * baseline becomes what was just written and IsDirty goes false without a second capture.
         *
         * @return false when the write failed — the baseline is then LEFT ALONE, so the document
         *   still knows it has unsaved work rather than claiming to be clean.
         */
        bool Save(const World& InWorld, const ComponentRegistry& InRegistry);

        /**
         * Save to a new path and adopt it (Save As). The MapId is unchanged: renaming the FILE a
         * map lives in does not rename the map its entities belong to, and quietly re-stamping
         * every entity would be a much bigger action than the menu item promises.
         */
        bool SaveAs(const OpaaxString& InAbsPath, const World& InWorld, const ComponentRegistry& InRegistry);

        /**
         * Replace the world's contents with the map at InAbsPath, then adopt it.
         *
         * Clears InWorld first — this is "open", not "merge". The world OBJECT survives: its
         * identity, its subsystems and the render target bound to it all stay put, because only
         * its entities are being replaced.
         *
         * @return false if the file could not be read, in which case the WORLD IS UNTOUCHED —
         *   a failed open must not cost the author what they already had.
         */
        bool Open(const OpaaxString& InAbsPath, World& InWorld, const ComponentRegistry& InRegistry);

        /**
         * @return true when the world no longer matches the baseline.
         *
         * O(entities + payloads) — call it when the answer is about to be shown or acted on, not
         * every frame. Always false when no map is open: there is nothing to be dirty against.
         */
        bool IsDirty(const World& InWorld, const ComponentRegistry& InRegistry) const;

        // =============================================================================
        // Get
        // =============================================================================
    public:
        bool               HasMap()   const noexcept { return !m_AbsPath.IsEmpty(); }
        const OpaaxString& AbsPath()  const noexcept { return m_AbsPath; }
        MapId              GetMapId() const noexcept { return m_MapId; }

        /** Just the file name, for the menu bar — "Main.opaaxmap". Empty when no map is open. */
        OpaaxString FileName() const;

        // =============================================================================
        // Members
        // =============================================================================
    private:
        /** The world serialized exactly as MapFile would write it — the thing IsDirty compares against. */
        OpaaxString Serialize(const World& InWorld, const ComponentRegistry& InRegistry) const;

        /**
         * The MapId InWorld's entities claim, or InAbsPath's file stem when the world is empty.
         * The one place that rule lives, so adopt and open cannot disagree about it.
         */
        static MapId DeriveMapId(const OpaaxString& InAbsPath, const World& InWorld);

        OpaaxString m_AbsPath;
        MapId       m_MapId;
        OpaaxString m_Baseline;
    };
}
