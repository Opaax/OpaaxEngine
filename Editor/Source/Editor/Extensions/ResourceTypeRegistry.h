#pragma once

#include "Core/OpaaxTypes.h"                    // TFunction, TDynArray, Uint64, Move
#include "Core/String/OpaaxString.hpp"
#include "Core/String/OpaaxStringID.hpp"
#include "Engine/Subsystems/Resources/ResourceFormat.h"   // CResourceFormat — what may carry chrome
#include "Engine/Subsystems/Resources/ResourceTypeID.hpp" // the key: one id per resource type
#include "Editor/Resources/ResourceScan.h"      // ResourceFile (the callback's argument)
#include "Editor/Resources/ResourcePreviewClaim.h" // FResourcePreviewOpen — the preview facet's type

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
    // ResourceTypeDesc — the EDITOR's half of one resource type: how it looks and what a
    //   double-click does.
    //
    //   Keyed by ResourceTypeID, NOT by extension. The engine's ResourceFormatRegistry owns
    //   extension -> type (many-to-one), so a texture claiming .png/.jpg/.tga is ONE entry here
    //   with one icon and one action, and adding .webp never touches the editor.
    //
    //   NO type erasure, deliberately: DrawerRegistry needs a template because TComponent is a
    //   type the editor cannot name, but chrome is two labels and a closure. A template would be
    //   ceremony with nothing to erase (user decision, M2d plan §1.2).
    // =============================================================================
    struct ResourceTypeDesc
    {
        Uint32               TypeId = 0;    // ResourceTypeID::Get<T>()
        OpaaxStringID        Label;         // OPTIONAL override; invalid => the format's own Label
        OpaaxString          Icon;          // OPTIONAL editor-assets-relative image; empty => the Glyph
        OpaaxString          Glyph;         // short text, "[W]" — the fallback when Icon is absent or missing
        FResourceActivate    OnActivate;
        FResourcePreviewOpen OnPreviewOpen; // OPTIONAL; absent => "no preview for this type"
    };

    class ResourceTypeRegistry;

    // =============================================================================
    // ResourceTypeBuilder — the chained tail of a Register<T>() call.
    //
    //   Holds an INDEX, never a ResourceTypeDesc& : m_Entries is a TDynArray, so the next
    //   Register reallocates and a stored reference would name freed memory the moment two
    //   registrations were split across statements (the string-pool bug's shape, I2).
    //
    //   A refused registration yields an invalid index whose setters are no-ops, so a bogus
    //   chain cannot write out of bounds and cannot make Count() lie.
    // =============================================================================
    class ResourceTypeBuilder
    {
        // =============================================================================
        // Ctor - Dtor
        // =============================================================================
    public:
        static constexpr Uint64 INVALID_INDEX = ~0ull;

        ResourceTypeBuilder(ResourceTypeRegistry* InRegistry, Uint64 InIndex) noexcept
            : m_Registry(InRegistry), m_Index(InIndex) {}

        // =============================================================================
        // Functions
        // =============================================================================
    public:
        /** Override the label the engine's format already carries — for a type the editor names differently. */
        ResourceTypeBuilder& SetLabel(OpaaxStringID InLabel);

        /**
         * The icon IMAGE, editor-assets-relative ("Icons/T_Map_Icon.png"). Searched in the PROJECT's
         * editor assets first, then the editor tool's own — so a game ships an icon for its own
         * resource type under the same relative name, and may override one of the editor's.
         */
        ResourceTypeBuilder& SetIcon(OpaaxString InIcon);

        /**
         * The short TEXT drawn when there is no icon image, "[L]".
         *
         * The fallback, not a lesser icon: a type that sets no image, or whose image file is
         * missing, still draws something rather than a blank card.
         */
        ResourceTypeBuilder& SetGlyph(OpaaxString InGlyph);

        /** What a double-click does. Omitted, the browser logs the activation and nothing else. */
        ResourceTypeBuilder& SetActivate(FResourceActivate InActivate);

        /**
         * What the Preview panel draws for this type, given the loaded resource.
         *
         * The TYPE IS NAMED HERE and nowhere else — the panel holds an IResourcePreviewClaim and
         * knows nothing about textures or fonts. That is the point of the facet: adding a
         * previewable type touches its own registration and no panel.
         *
         * @tparam TResource The resource type this chrome describes.
         * @param InDraw Runs every frame the entry is open, with a claim already held for it.
         */
        template<CResource TResource>
        ResourceTypeBuilder& SetPreview(typename TResourcePreviewClaim<TResource>::FDraw InDraw)
        {
            return SetPreviewOpen(MakePreviewOpener<TResource>(Move(InDraw)));
        }

        /** The type-erased half, for a caller that already built its opener. */
        ResourceTypeBuilder& SetPreviewOpen(FResourcePreviewOpen InOpen);

        // =============================================================================
        // Members
        // =============================================================================
    private:
        ResourceTypeRegistry* m_Registry = nullptr;
        Uint64                m_Index    = INVALID_INDEX;
    };

    // =============================================================================
    // ResourceTypeRegistry — the real storage behind EditorExtensionRegistrar::ResourceTypes()
    //   (Editor.md D10), replacing the M0 counts-only EditorRoute for this channel, as PanelRegistry
    //   (M2a) and DrawerRegistry (M2b) did for theirs.
    //
    //   It answers ONE question — "how does this resource type look, and what opens it?" — while
    //   the engine answers "which type is this file?". Adding a type touches no editor file beyond
    //   its own chrome, and adding an EXTENSION touches no editor file at all.
    //
    //   Registration STORES ONLY. It runs at the OnModulesRegistered seam, before Engine::Startup,
    //   so there is no context, no world and no scan yet — only the closure carries intent across
    //   that gap.
    // =============================================================================
    class ResourceTypeRegistry
    {
        // =============================================================================
        // Functions
        // =============================================================================
    public:
        /**
         * Register chrome for T, whose extensions the ENGINE already owns. Constrained on
         * CResourceFormat, so chrome for a type that declared no format is a compile error here
         * rather than an entry nothing can ever match.
         *
         * @tparam T A resource type carrying OPAAX_RESOURCE_FORMAT.
         * @return A builder for the optional icon / label / action. Chained, never stored.
         */
        template<CResourceFormat T>
        ResourceTypeBuilder Register()
        {
            return AddEntry(ResourceTypeID::Get<T>());
        }

        /** @return The chrome registered for InTypeId, or nullptr — an O(n) walk over a handful of entries, on an integer compare. */
        const ResourceTypeDesc* Find(Uint32 InTypeId) const
        {
            for (const ResourceTypeDesc& lEntry : m_Entries)
            {
                if (lEntry.TypeId == InTypeId)
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
        // Functions
        // =============================================================================
    private:
        friend class ResourceTypeBuilder;

        /** Store one entry. A second registration for the same type is dropped — the first wins. */
        ResourceTypeBuilder AddEntry(Uint32 InTypeId)
        {
            if (Find(InTypeId) != nullptr)
            {
                return ResourceTypeBuilder(this, ResourceTypeBuilder::INVALID_INDEX);
            }

            m_Entries.emplace_back(ResourceTypeDesc{ .TypeId = InTypeId });
            return ResourceTypeBuilder(this, static_cast<Uint64>(m_Entries.size()) - 1);
        }

        ResourceTypeDesc* EntryAt(Uint64 InIndex)
        {
            return (InIndex < m_Entries.size()) ? &m_Entries[InIndex] : nullptr;
        }

        // =============================================================================
        // Members
        // =============================================================================
    private:
        TDynArray<ResourceTypeDesc> m_Entries;
    };

    // =============================================================================
    // ResourceTypeBuilder — bodies, now that the registry is complete.
    // =============================================================================
    inline ResourceTypeBuilder& ResourceTypeBuilder::SetLabel(OpaaxStringID InLabel)
    {
        if (ResourceTypeDesc* lEntry = m_Registry != nullptr ? m_Registry->EntryAt(m_Index) : nullptr)
        {
            lEntry->Label = InLabel;
        }

        return *this;
    }

    inline ResourceTypeBuilder& ResourceTypeBuilder::SetIcon(OpaaxString InIcon)
    {
        if (ResourceTypeDesc* lEntry = m_Registry != nullptr ? m_Registry->EntryAt(m_Index) : nullptr)
        {
            lEntry->Icon = Move(InIcon);
        }

        return *this;
    }

    inline ResourceTypeBuilder& ResourceTypeBuilder::SetGlyph(OpaaxString InGlyph)
    {
        if (ResourceTypeDesc* lEntry = m_Registry != nullptr ? m_Registry->EntryAt(m_Index) : nullptr)
        {
            lEntry->Glyph = Move(InGlyph);
        }

        return *this;
    }

    inline ResourceTypeBuilder& ResourceTypeBuilder::SetActivate(FResourceActivate InActivate)
    {
        if (ResourceTypeDesc* lEntry = m_Registry != nullptr ? m_Registry->EntryAt(m_Index) : nullptr)
        {
            lEntry->OnActivate = Move(InActivate);
        }

        return *this;
    }

    inline ResourceTypeBuilder& ResourceTypeBuilder::SetPreviewOpen(FResourcePreviewOpen InOpen)
    {
        if (ResourceTypeDesc* lEntry = m_Registry != nullptr ? m_Registry->EntryAt(m_Index) : nullptr)
        {
            lEntry->OnPreviewOpen = Move(InOpen);
        }

        return *this;
    }
}
