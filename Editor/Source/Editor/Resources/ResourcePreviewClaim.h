#pragma once

#include "Core/OpaaxTypes.h"
#include "Resources/ResourceManager.h"   // Load<T>
#include "Editor/Resources/ResourceScan.h"                 // ResourceFile

namespace Opaax::Editor
{
    struct EditorContext;   // only named: Draw passes it through

    // =============================================================================
    // ResourcePreviewClaim — one open preview: the load keeping its resource in memory, and the
    //   function that draws it. The Preview panel holds TUniquePtr<IResourcePreviewClaim> and names
    //   no resource type. The load and the drawing stay together: reloading every frame could reload
    //   from disk whenever garbage collection ran in between.
    // =============================================================================

    /** One open preview, type erased. Built once when the entry opens, drawn every frame after. */
    class IResourcePreviewClaim
    {
    public:
        virtual ~IResourcePreviewClaim() = default;

        /** Draws this resource's content. Called inside the entry's section, every frame it is open. */
        virtual void Draw(EditorContext& InContext) = 0;
    };

    /**
     * What a preview registration gives the panel when an entry opens: the claim, or nullptr when the
     * resource could not be loaded. Takes the manager, so this header only forward-declares the context.
     */
    using FResourcePreviewOpen =
        TFunction<TUniquePtr<IResourcePreviewClaim>(ResourceManager&, const ResourceFile&)>;

    /**
     * The one implementation: a typed claim plus the typed draw function registered with it.
     * @tparam TResource The resource type, named only at the registration site
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
            // Null while an async load is in flight: nothing to show this frame (not an error).
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
     * Builds the opener stored by a SetPreview registration.
     * @tparam TResource The resource type this previews
     * @param InDraw What to draw, given the loaded resource. Runs every frame the entry is open
     * @return The opener. It returns nullptr for a file that does not load (the panel says so)
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
