#pragma once

#include "Core/OpaaxTypes.h"                // TFunction, TDynArray, Uint64, Move
#include "Core/String/OpaaxString.hpp"

namespace Opaax::Editor
{
    struct EditorContext;

    /**
     * What a menu entry DOES when clicked.
     *
     * Takes the context for the same reason FResourceActivate does (D3): a command's whole job is
     * to act on the world, the selection or the resources, and the locator is not available to it.
     * This is also why the M0 placeholder could not survive the route going real — it was a
     * zero-argument lambda, which does not convert, exactly as predicted when the skeleton was
     * left behind.
     */
    using FMenuCommand = TFunction<void(EditorContext&)>;

    // =============================================================================
    // MenuEntry — one command and where it lives in the bar.
    //
    //   Path is slash-separated, top level first: "Tools/Validate Sandbox", "File/Exit". The
    //   LAST segment is the item's label; everything before it is the menu chain.
    // =============================================================================
    struct MenuEntry
    {
        OpaaxString  Path;
        FMenuCommand Command;
    };

    /**
     * The InDepth'th slash-separated segment of InPath, or EMPTY when the path has no such
     * segment. "Tools/Validate Sandbox" -> depth 0 is "Tools", depth 1 is "Validate Sandbox".
     *
     * Free and header-inline because both the registry's own tests and the menu-bar renderer
     * need the same answer, and a path convention that two places implement separately is a
     * path convention with two meanings.
     */
    inline OpaaxString MenuPathSegment(const OpaaxString& InPath, Uint32 InDepth)
    {
        const OpaaxString lPath  = InPath;
        const char* const lChars = lPath.CStr();
        const Uint32      lLen   = lPath.GetLength();

        Uint32 lStart = 0;
        Uint32 lDepth = 0;

        for (Uint32 lIndex = 0; lIndex <= lLen; ++lIndex)
        {
            const bool bEnd = (lIndex == lLen) || (lChars[lIndex] == '/');
            if (!bEnd) { continue; }

            if (lDepth == InDepth)
            {
                OpaaxString lSegment;
                for (Uint32 lCopy = lStart; lCopy < lIndex; ++lCopy)
                {
                    const char lOne[2] = { lChars[lCopy], '\0' };
                    lSegment.Append(lOne);
                }
                return lSegment;
            }

            ++lDepth;
            lStart = lIndex + 1;
        }

        return OpaaxString();
    }

    /** @return true when InDepth names the LAST segment of InPath — i.e. a command, not a submenu. */
    inline bool IsMenuPathLeaf(const OpaaxString& InPath, Uint32 InDepth)
    {
        return MenuPathSegment(InPath, InDepth + 1).IsEmpty();
    }

    // =============================================================================
    // MenuRegistry — the real storage behind EditorExtensionRegistrar::Menus() (Editor.md D10),
    //   and the LAST of the five routes to graduate off the counts-only EditorRoute — which is
    //   deleted in the same change, since this was its only remaining user.
    //
    //   STORAGE IS A FLAT ARRAY IN REGISTRATION ORDER, and the nesting is computed at DRAW time.
    //   A tree built at registration would be a second structure to keep consistent with the
    //   paths, for a handful of entries that are walked once a frame — and PanelRegistry /
    //   ResourceTypeRegistry both already have the shape where storing is trivial and the
    //   consumer does the interesting part.
    //
    //   Registration STORES ONLY. It runs at the OnModulesRegistered seam, before Engine::Startup,
    //   so there is no context, no world and no UI yet — the closure is what carries the intent
    //   across that gap.
    // =============================================================================
    class MenuRegistry
    {
        // =============================================================================
        // Functions
        // =============================================================================
    public:
        /**
         * Store one command under InPath.
         *
         * An entry with an empty path or no command is DROPPED: neither could ever be shown or
         * invoked, and keeping it would only make Count() lie — the same rule
         * ResourceTypeRegistry applies to a type with no extension.
         */
        void Register(const char* InPath, FMenuCommand InCommand)
        {
            if (InPath == nullptr || *InPath == '\0' || !InCommand)
            {
                return;
            }

            m_Entries.push_back(MenuEntry{ OpaaxString(InPath), Move(InCommand) });
        }

        // =============================================================================
        // Get - Set
    public:
        /** @return The registered commands in registration order (= display order within a menu). */
        const TDynArray<MenuEntry>& Entries() const noexcept { return m_Entries; }

        /** @return How many commands were registered — same signature EditorRoute had, so the seal log is unchanged. */
        Uint64 Count() const noexcept { return static_cast<Uint64>(m_Entries.size()); }
        // End Get - Set
        // =============================================================================

        // =============================================================================
        // Members
        // =============================================================================
    private:
        TDynArray<MenuEntry> m_Entries;
    };
}
