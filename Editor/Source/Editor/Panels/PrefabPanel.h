#pragma once

#include "Application/Services/ILogger.h"
#include "Core/Maths/MathTypes.h"
#include "Core/OpaaxTypes.h"
#include "Editor/Panels/IEditorPanel.h"
#include "World/Entity/EntityTypes.h"

namespace Opaax
{
    class IFramebuffer;
    class OffscreenRenderTarget;

    OPAAX_LOG_CATEGORY(PrefabPanel);
}

namespace Opaax::Editor
{
    struct EditorContext;
    struct EditorImage;

    // =============================================================================
    // PrefabPanel — a prefab edited in a world of its own (⑦-C P6).
    //
    //   THE THIRD VIEW, and the first that draws a world which is not the active one. **MV1** made a
    //   frame a list of views, but every one of them drew whatever `RenderFrame` resolved once — so
    //   multi-view meant N views of ONE world, which is all the Camera Preview needed. A prefab is
    //   edited in a world that is deliberately never active, which is why P6 had to give a view a
    //   `Source`. This is **MV**'s named "asset preview WORLD" growth point, whose stated trigger
    //   was ⑦'s prefabs.
    //
    //   SELF-CONTAINED, like every other document panel here (`SpriteSheetPanel`,
    //   `AnimationClipPanel`, `InputMappingContextPanel`): its own tree, its own properties, its own
    //   Save. That is what keeps it out of the way of `EditorSelection`, the gizmo and the
    //   Inspector, all of which are bound to the ACTIVE world and would each have needed a second
    //   target otherwise.
    //
    //   ITS SELECTION IS ITS OWN, one `EntityID`, never `EditorSelection`. A handle from another
    //   world means nothing — and entt REUSES handles, so a shared selection could silently resolve
    //   to a different entity that happens to exist in the level (**MV4**'s reason, one panel over).
    //
    //   NO GIZMO AND NO UNDO INSIDE IT YET, and both are stated rather than discovered: editing is
    //   the Inspector-style property form, and the undo stack belongs to the level's world (**UN1**).
    //   The Config panel already sets that precedent and the user accepted it there.
    // =============================================================================
    class PrefabPanel final : public IEditorPanel
    {
        // =========================================================================
        // CTORS - DTORS
        // =========================================================================
    public:
        explicit PrefabPanel(EditorContext& InContext) noexcept : m_Context(InContext) {}
        ~PrefabPanel() override;

        // =========================================================================
        // Functions
        // =========================================================================
    public:
        /** The registry key, the ImGui window label and the dock key, stated once (**MR2c**). */
        OPAAX_EDITOR_PANEL_NAME(Prefab)

        // =========================================================================
        // Internal
        // =========================================================================
    private:
        /** One row per entity of the prefab, and the click that selects one. */
        void DrawHierarchy();

        /** The selected entity's components, through the SAME drawer registry the Inspector uses. */
        void DrawProperties();

        /** The rendered prefab, and the deferred resize ViewportPanel's shape already uses. */
        void DrawPreview();

        // =========================================================================
        // Override
        // =========================================================================
        //~Begin IEditorPanel interface
    public:
        void            Startup()     override {}
        void            OnPreRender() override;
        void            DrawContents() override;
        void            Shutdown()    override;
        //~End IEditorPanel interface

        // =========================================================================
        // Members
        // =========================================================================
    private:
        EditorContext& m_Context;

        TUniquePtr<IFramebuffer>          m_Framebuffer;
        TUniquePtr<OffscreenRenderTarget> m_RenderTarget;

        /** Measured in DrawContents, applied by OnPreRender next frame — the deferred resize. */
        Vector2u32 m_Size        = { 512, 512 };
        Vector2u32 m_PendingSize = { 512, 512 };

        /** How much world the preview frames. Fixed for now; framing the contents is a growth point. */
        float m_OrthoSize = 400.f;

        /** THIS panel's selection — see the class note on why it is not EditorSelection. */
        EntityID m_Selected = entt::null;
    };
}
