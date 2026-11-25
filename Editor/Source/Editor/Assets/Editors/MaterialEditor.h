#pragma once

#include "Core/Maths/MathTypes.h"   // Vector2F
#include "Core/String/OpaaxString.hpp"
#include "Editor/Assets/IAssetEditor.h"
#include "Renderer/Materials/SpriteShader.h"
#include "Resources/Asset/AssetHandle.h"

namespace Opaax
{
    class Material2D;
}

namespace Opaax::Editor
{
    struct EditorContext;

    // =============================================================================
    // MaterialEditor — a Material2D's window: its properties, the shader's values shown under the
    //   names its `// @param` lines give them (the others hidden), the shader's refused lines, and the
    //   defaults a newly picked shader asks for (MaterialEditing).
    // =============================================================================
    class MaterialEditor final : public IAssetEditor
    {
    public:
        explicit MaterialEditor(EditorContext& InContext) : m_Context(InContext) {}

        //~Begin IAssetEditor interface
        void DrawContents(AssetDocument& InDocument) override;
        Vector2F GetDefaultSize() const override { return { 480.f, 620.f }; }
        //~End IAssetEditor interface

    private:
        /** The material's shader, loaded when its path changes; null when it names none or did not load. */
        const SpriteShader* ResolveShader(const Material2D& InMaterial);

        /** One line on the shader: none, not loaded, what it declares; then its refused lines. */
        void DrawShaderSummary(const Material2D& InMaterial, const SpriteShader* InShader);

        EditorContext&             m_Context;
        TAssetHandle<SpriteShader> m_Shader;
        OpaaxString                m_ShaderPath;       // the path m_Shader was loaded from
        OpaaxString                m_LastShaderPath;   // the Shader field last frame
        bool                       m_bSeenShaderField = false;
    };
}
