#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/Maths/MathTypes.h"

namespace Opaax
{
    class Material2D;

    // =============================================================================
    // MaterialBlock — what a game's sprite shader reads of its material and of the frame, the
    //   MaterialUBO of Engine/Assets/Shaders/SpriteMaterial.glsl (std140, binding 8). Uploaded for
    //   each batch drawn with a material's shader. The material's two textures ride in the batch's
    //   samplers, at MATERIAL_TEXTURE_SLOT_0 and _1.
    // =============================================================================

    /** The binding point of MaterialUBO. */
    inline constexpr Uint32 MATERIAL_UBO_BINDING = 8;

    /** The samplers of u_Textures[16] a material's Texture0 and Texture1 take. */
    inline constexpr Uint32 MATERIAL_TEXTURE_SLOT_0 = 12;
    inline constexpr Uint32 MATERIAL_TEXTURE_SLOT_1 = 13;

    struct MaterialBlock
    {
        Vector4F Time      = { 0.f, 0.f, 0.f, 0.f };   // x: the world's time, y: the app's (seconds)
        Vector4F Params[2] = {};                       // Param0..3, Param4..7
        Vector4F Colors[2] = {};                       // Color0, Color1, as written (screen colour)
    };

    static_assert(sizeof(MaterialBlock) == 5 * sizeof(Vector4F), "MaterialBlock must match MaterialUBO (std140)");

    /**
     * InMaterial's values and the times, laid out for MaterialUBO.
     * @param InWorldTime The world's time (stands still while it is paused), seconds
     * @param InAppTime The app's time (always runs), seconds
     */
    MaterialBlock MakeMaterialBlock(const Material2D& InMaterial, double InWorldTime, double InAppTime) noexcept;
}
