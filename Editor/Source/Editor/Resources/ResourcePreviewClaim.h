#pragma once

#include "Core/OpaaxTypes.h"
#include "Engine/Subsystems/Resources/ResourceManager.h"   // Load<T> — the claim this owns
#include "Editor/Resources/ResourceScan.h"                 // ResourceFile

namespace Opaax::Editor
{
    struct EditorContext;   // only NAMED — Draw passes it straight through

    // =============================================================================
    // ResourcePreviewClaim — ONE open preview: the claim keeping its resource resident, and the
    //   closure that draws it.
    //
    //   THE LIVE OBJECT of the preview route, and that is the whole point (**MR2g**: every extension
    //   route is registry -> live object -> backend). The Preview panel used to hold
    //   `ResourceRef<TextureResource>` and branch on the type id, with its own comment saying the
    //   second previewable type should turn that into a chrome facet rather than a chain of ifs.
    //   This is that facet: the panel now holds `TUniquePtr<IResourcePreviewClaim>` and names no
    //   resource type at all.
    //
    //   THE CLAIM AND THE DRAWING TRAVEL TOGETHER because they must: ResourcePool defers an unload
    //   only to the next CollectGarbage pump, so a drawer that re-Loaded every frame would re-bake a
    //   font from disk the first time the pump landed between two frames. Whoever draws it holds it.
    // =============================================================================

    /** One open preview, type erased. Built once when the entry opens, drawn every frame after. */
    class IResourcePreviewClaim
    {
    public:
        virtual ~IResourcePreviewClaim() = default;

        /** Draw this resource's content. Called inside the entry's section, every frame it is open. */
        virtual void Draw(EditorContext& InContext) = 0;
    };

    /**
     * What a chrome registration hands the panel when an entry opens: the claim, or nullptr when the
     * resource could not be loaded.
     *
     * It takes the MANAGER rather than the EditorContext so this header needs only a forward
     * declaration of the context — the concrete claim below passes the context straight through to
     * the drawer without ever touching a member of it.
     */
    using FResourcePreviewOpen =
        TFunction<TUniquePtr<IResourcePreviewClaim>(ResourceManager&, const ResourceFile&)>;

    /**
     * The one implementation: a typed claim plus the typed drawer registered beside it.
     *
     * @tparam TResource The resource type. Named ONLY at the registration site.
     */
    template<CResource TResource>
    class TResourcePreviewClaim final : public IResourcePreviewClaim
    {
        // =============================================================================
        // Ctor - Dtor
        // =============================================================================
    public:
        using FDraw = TFunction<void(EditorContext&, const TResource&)>;

        TResourcePreviewClaim(ResourceRef<TResource> InRef, FDraw InDraw)
            : m_Ref(Move(InRef))
            , m_Draw(Move(InDraw))
        {
        }

        // =============================================================================
        // Override
        // =============================================================================
        //~Begin IResourcePreviewClaim interface
    public:
        void Draw(EditorContext& InContext) override
        {
            // Null while an async load is still in flight — the entry simply has nothing to show
            // this frame, which is not an error and not worth a per-frame line.
            if (const TResource* lResource = m_Ref.Get())
            {
                m_Draw(InContext, *lResource);
            }
        }
        //~End IResourcePreviewClaim interface

        // =============================================================================
        // Members
        // =============================================================================
    private:
        ResourceRef<TResource> m_Ref;
        FDraw                  m_Draw;
    };

    /**
     * Build the opener a `SetPreview` registration stores.
     *
     * @tparam TResource The resource type this chrome previews.
     * @param InDraw What to draw, given the loaded resource. Runs every frame the entry is open.
     * @return The opener; it answers nullptr for a file that does not load, and the panel says so.
     */
    template<CResource TResource>
    FResourcePreviewOpen MakePreviewOpener(typename TResourcePreviewClaim<TResource>::FDraw InDraw)
    {
        return [lDraw = Move(InDraw)](ResourceManager& InResources, const ResourceFile& InFile)
                   -> TUniquePtr<IResourcePreviewClaim>
        {
            ResourceRef<TResource> lRef = InResources.Load<TResource>(InFile.AbsPath.CStr());

            if (!lRef.IsValid())
            {
                return nullptr;
            }

            return MakeUnique<TResourcePreviewClaim<TResource>>(Move(lRef), lDraw);
        };
    }
}
