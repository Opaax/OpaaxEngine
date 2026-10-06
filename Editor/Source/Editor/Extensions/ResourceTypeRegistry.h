#pragma once

#include "Core/OpaaxTypes.h"                    // TFunction, TDynArray, Uint64, Move
#include "Core/String/OpaaxString.hpp"
#include "Core/String/OpaaxStringID.hpp"
#include "Resources/ResourceFormat.h"   // CResourceFormat
#include "Resources/ResourceTypeID.hpp" // ResourceTypeID
#include "Editor/Resources/ResourceScan.h"      // ResourceFile (the callback's argument)
#include "Editor/Resources/ResourcePreviewClaim.h" // FResourcePreviewOpen

namespace Opaax::Editor
{
    struct EditorContext;

    /**
     * What double-clicking a file of this type does. Receives the context and the file. Optional: a
     * type may only want an icon and a label.
     */
    using FResourceActivate = TFunction<void(EditorContext&, const ResourceFile&)>;

    // =============================================================================
    // ResourceTypeDesc — the editor's side of one resource type: how it looks and what a double-click
    //   does. Keyed by ResourceTypeID, not extension: the engine maps extensions to types, so one
    //   texture entry covers .png/.jpg/.tga.
    // =============================================================================
    struct ResourceTypeDesc
    {
        Uint32               TypeId = 0;    // ResourceTypeID::Get<T>()
        OpaaxStringID        Label;         // optional override; invalid uses the format's Label
        OpaaxString          Icon;          // optional editor-assets-relative image; empty uses the Glyph
        OpaaxString          Glyph;         // short text, "[W]", used when Icon is absent or missing
        FResourceActivate    OnActivate;
        FResourcePreviewOpen OnPreviewOpen; // optional; absent means no preview
    };

    class ResourceTypeRegistry;

    // =============================================================================
    // ResourceTypeBuilder — the chained tail of a Register<T>() call. Holds an index, not a reference
    //   (the next Register may reallocate the array). A refused registration gets an invalid index
    //   whose setters do nothing.
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
        /** Overrides the label from the engine's format. */
        ResourceTypeBuilder& SetLabel(OpaaxStringID InLabel);

        /**
         * The icon image, relative to editor assets ("Icons/T_Map_Icon.png"). Looked up in the project's
         * editor assets first, then the editor's own, so a game can add or override icons.
         */
        ResourceTypeBuilder& SetIcon(OpaaxString InIcon);

        /**
         * The short text drawn when there is no icon image ("[L]"), or the image is missing.
         */
        ResourceTypeBuilder& SetGlyph(OpaaxString InGlyph);

        /** What a double-click does. If omitted, the browser only logs the activation. */
        ResourceTypeBuilder& SetActivate(FResourceActivate InActivate);

        /**
         * What the Preview panel draws for this type, given the loaded resource. The type is named only
         * here; the panel knows nothing about textures or fonts.
         * @tparam TResource The resource type
         * @param InDraw Runs every frame the entry is open, with the resource already loaded
         */
        template<CResource TResource>
        ResourceTypeBuilder& SetPreview(typename TResourcePreviewClaim<TResource>::FDraw InDraw)
        {
            return SetPreviewOpen(MakePreviewOpener<TResource>(Move(InDraw)));
        }

        /** The type-erased form, for a caller that already built its opener. */
        ResourceTypeBuilder& SetPreviewOpen(FResourcePreviewOpen InOpen);

        // =============================================================================
        // Members
        // =============================================================================
    private:
        ResourceTypeRegistry* m_Registry = nullptr;
        Uint64                m_Index    = INVALID_INDEX;
    };

    // =============================================================================
    // ResourceTypeRegistry — the storage behind EditorExtensionRegistrar::ResourceTypes(). Answers
    //   "how does this type look, and what opens it?"; the engine answers "which type is this file?".
    //   Registration only stores (it runs before any context, world or scan exists).
    // =============================================================================
    class ResourceTypeRegistry
    {
        // =============================================================================
        // Functions
        // =============================================================================
    public:
        /**
         * Registers the editor side of T. Constrained on CResourceFormat, so a type with no format is a
         * compile error.
         * @tparam T A resource type with OPAAX_RESOURCE_FORMAT
         * @return A builder for the optional icon / label / action. Chain it, do not store it
         */
        template<CResourceFormat T>
        ResourceTypeBuilder Register()
        {
            return AddEntry(ResourceTypeID::Get<T>());
        }

        /** @return The entry for InTypeId, or nullptr (linear search over a few entries). */
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
        /** @return The registered types, in registration order. */
        const TDynArray<ResourceTypeDesc>& Entries() const noexcept { return m_Entries; }

        /** @return How many types were registered. */
        Uint64 Count() const noexcept { return static_cast<Uint64>(m_Entries.size()); }
        // End Get - Set
        // =============================================================================

        // =============================================================================
        // Functions
        // =============================================================================
    private:
        friend class ResourceTypeBuilder;

        /** Stores one entry. A second registration for the same type is dropped (the first wins). */
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
