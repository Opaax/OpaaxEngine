#pragma once

#include "Core/Maths/MathTypes.h"   // Vector2F
#include "Core/String/OpaaxString.hpp"
#include "Editor/Assets/Editors/MaterialEditing.h"
#include "Editor/Assets/IAssetEditor.h"
#include "Renderer/Materials/SpriteShader.h"
#include "Resources/Asset/AssetHandle.h"
#include "World/Entity/Entity.h"

namespace Opaax
{
    class IFramebuffer;
    class Material2D;
    class OffscreenRenderTarget;
    class World;
}

namespace Opaax::Editor
{
    struct EditorContext;

    // =============================================================================
    // MaterialEditor — a Material2D's window: a preview of the material on a square, then its
    //   properties, the shader's values shown under the names its `// @param` lines give them (the
    //   others hidden), the shader's refused lines, and the defaults a newly picked shader asks for
    //   (MaterialEditing). The edited values reach what is drawn as they change (MaterialLiveEdit).
    //   The preview is a world of its own (Edit mode), drawn into a framebuffer before the frame.
    // =============================================================================
    class MaterialEditor final : public IAssetEditor
    {
    public:
        explicit MaterialEditor(EditorContext& InContext);

        /** Closing with unsaved values gives the loaded copy the file's back; the preview goes. */
        ~MaterialEditor() override;

        //~Begin IAssetEditor interface
        void DrawContents(AssetDocument& InDocument) override;
        void OnPreRender() override;
        Vector2F GetDefaultSize() const override { return { 480.f, 600.f }; }
        //~End IAssetEditor interface

        // =============================================================================
        // Preview
        // =============================================================================
    public:
        /** The preview's height in pixels; it takes the window's width. */
        static constexpr float PREVIEW_HEIGHT = 200.f;

        /** The texture the preview square shows, until another is dropped on it. */
        static constexpr const char* DEFAULT_PREVIEW_TEXTURE = "/Engine/Textures/T_Square_256_White.png";

        /** The framebuffer the preview is drawn into; null before the first preview. For automation. */
        IFramebuffer* GetPreviewFramebuffer() const noexcept { return m_Framebuffer.get(); }

    private:
        /** The material's shader, loaded when its path changes; null when it names none or did not load. */
        const SpriteShader* ResolveShader(const Material2D& InMaterial);

        /** One line on the shader: none, not loaded, what it declares; then its refused lines. */
        void DrawShaderSummary(const Material2D& InMaterial, const SpriteShader* InShader);

        /** The preview image and its settings (texture, world time, lit). */
        void DrawPreview();

        /** Makes the preview world (a square with this material, and an Environment when lit). */
        bool EnsurePreviewWorld();

        /** The preview world is going (the editor is closing, or it was destroyed elsewhere). */
        void HandleWorldDestroyed(World* InWorld);

        EditorContext&                    m_Context;
        MaterialEditing::MaterialLiveEdit m_LiveEdit;           // the edited values reach what is drawn
        bool                              m_bBound   = false;
        bool                              m_bUnsaved = false;   // as of the last frame drawn

        TAssetHandle<SpriteShader>        m_Shader;
        OpaaxString                       m_ShaderPath;       // the path m_Shader was loaded from
        OpaaxString                       m_LastShaderPath;   // the Shader field last frame
        bool                              m_bSeenShaderField = false;

        OpaaxString                       m_AssetPath;        // the material, as a sprite names it
        World*                            m_PreviewWorld = nullptr;
        Entity                            m_PreviewSquare;
        Entity                            m_PreviewEnvironment;
        TUniquePtr<IFramebuffer>          m_Framebuffer;
        TUniquePtr<OffscreenRenderTarget> m_RenderTarget;
        Uint32                            m_PreviewWidth  = 0;   // the framebuffer's size
        Uint32                            m_PreviewHeight = 0;
        Uint32                            m_WantedWidth   = 0;   // measured while drawing, applied before the next frame
        OpaaxString                       m_PreviewTexture{ DEFAULT_PREVIEW_TEXTURE };
        bool                              m_bWorldTimeRuns = true;
        bool                              m_bLit           = false;
    };
}
