#pragma once

#include "Core/OpaaxTypes.h"                    // TFunction, TDynArray, Uint64, Move
#include "Core/String/OpaaxString.hpp"
#include "Core/String/OpaaxStringID.hpp"
#include "Editor/Resources/ResourceScan.h"      // ResourceFile (the callback's argument) + NormalizeExtension

namespace Opaax::Editor
{
    struct EditorContext;

    /**
     * What a double-click on a file of this type does. Receives the context (D3 — a game's action
     * reaches the world/resources through it, never through the locator) and the file that was
     * activated. Optional: an empty action is a perfectly good registration for a type that only wants
     * an icon and a label.
     */
    using FResourceActivate = TFunction<void(EditorContext&, const ResourceFile&)>;

    // =============================================================================
    // ResourceTypeDesc — one registered file type, keyed by extension.
    //
    //   NO type erasure here, deliberately: DrawerRegistry needs a template because TComponent is a
    //   type the editor cannot name, but a file type is a string key plus two labels and a closure.
    //   A template would be ceremony with nothing to erase (user decision, M2d plan §1.2).
    // =============================================================================
    struct ResourceTypeDesc
    {
        OpaaxStringID     Extension;    // ".wave" — normalized by Register, so registrants can be sloppy
        OpaaxStringID     Label;        // "Wave Definition" — shown via CStr()
        OpaaxString       Icon;         // short text glyph, "[W]" — presentation only, never compared
        FResourceActivate OnActivate;
    };

    // =============================================================================
    // ResourceTypeRegistry — the real storage behind EditorExtensionRegistrar::ResourceTypes()
    //   (Editor.md D10), replacing the M0 counts-only EditorRoute for this channel, as PanelRegistry
    //   (M2a) and DrawerRegistry (M2b) did for theirs.
    //
    //   The browser asks it exactly one question — "what is this extension?" — so a file type stays a
    //   property of the FILE, not of the panel: adding a type never touches the panel, which is the
    //   whole point of the route.
    //
    //   Registration STORES ONLY. It runs at the OnModulesRegistered seam, before Engine::Startup, so
    //   there is no context, no world and no scan yet — only the closure carries intent across that gap.
    // =============================================================================
    class ResourceTypeRegistry
    {
        // =============================================================================
        // Functions
        // =============================================================================
    public:
        /**
         * Store one type, normalizing its extension first so registration and scanning agree on the id
         * (§ NormalizeExtension). A descriptor with an empty extension is dropped — it could never
         * match a file, and keeping it would only make Count() lie.
         */
        void Register(ResourceTypeDesc InDesc)
        {
            // Validity is checked BEFORE the round-trip, not after: OpaaxStringID::ToString() answers
            // "None" for an invalid id, which would normalize into a perfectly valid ".none" type and
            // silently match nothing forever.
            if (!InDesc.Extension.IsValid())
            {
                return;
            }

            InDesc.Extension = NormalizeExtension(InDesc.Extension.ToString());
            m_Entries.push_back(Move(InDesc));
        }

        /** @return The type registered for InExtension, or nullptr — an O(n) walk over a handful of entries, on an integer compare. */
        const ResourceTypeDesc* Find(OpaaxStringID InExtension) const
        {
            if (!InExtension.IsValid())
            {
                return nullptr;
            }

            for (const ResourceTypeDesc& lEntry : m_Entries)
            {
                if (lEntry.Extension == InExtension)
                {
                    return &lEntry;
                }
            }

            return nullptr;
        }

        // =============================================================================
        // Get - Set
    public:
        /** @return The registered types in registration order. */
        const TDynArray<ResourceTypeDesc>& Entries() const noexcept { return m_Entries; }

        /** @return How many types were registered — same signature EditorRoute had, so the seal log is unchanged. */
        Uint64 Count() const noexcept { return static_cast<Uint64>(m_Entries.size()); }
        // End Get - Set
        // =============================================================================

        // =============================================================================
        // Members
        // =============================================================================
    private:
        TDynArray<ResourceTypeDesc> m_Entries;
    };
}
