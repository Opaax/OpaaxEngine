#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"
#include "Core/String/OpaaxStringView.hpp"

namespace Opaax
{
    // =============================================================================
    // ShaderParams — what a game's sprite shader says it reads of its material, one comment line
    //   per slot, anywhere in its .glsl:
    //
    //     // @param Param0   EdgeCut   "Where the fog is cut"   range(0, 1) default(0.5)
    //     // @param Color0   ChalkTint "Tint of the grain"
    //     // @param Texture0 Noise     "Chalk noise"
    //
    //   The material's window shows the declared slots under these names (and hides the others),
    //   and the shader's code can use the names (SpriteShader defines them). range() and default()
    //   are for the numbers (Param0..7). Pure text: no file, no GPU.
    // =============================================================================

    /** A material value a shader can read: the slots of Material2D. */
    enum class EShaderParamSlot : Uint8
    {
        Param0, Param1, Param2, Param3, Param4, Param5, Param6, Param7,
        Color0, Color1,
        Texture0, Texture1,
        Count
    };

    /** One declared slot. */
    struct ShaderParam
    {
        EShaderParamSlot Slot = EShaderParamSlot::Param0;
        OpaaxString      Name;       // what the shader and the editor call it
        OpaaxString      Tooltip;    // empty when none was given
        bool             bHasRange   = false;
        float            RangeMin    = 0.f;
        float            RangeMax    = 0.f;
        bool             bHasDefault = false;
        float            Default     = 0.f;
    };

    /** A shader's declarations, and why the lines that were refused were. */
    struct ShaderParamList
    {
        TDynArray<ShaderParam> Params;   // in the order declared
        TDynArray<OpaaxString> Errors;   // "line 3: ..." for each refused line

        /** The declaration of InSlot, or null when the shader does not declare it. */
        const ShaderParam* Find(EShaderParamSlot InSlot) const noexcept;
    };

    namespace ShaderParams
    {
        /** Reads every `// @param` line of InSource. A refused line is left out, with its reason. */
        ShaderParamList Parse(OpaaxStringView InSource);

        /** The slot's name as Material2D's field: "Param0", "Color1", "Texture0". */
        const char* SlotName(EShaderParamSlot InSlot) noexcept;

        /** The slot named InName (a Material2D field name), or false. */
        bool SlotFromName(OpaaxStringView InName, EShaderParamSlot& OutSlot) noexcept;

        /** True for Param0..7. */
        bool IsNumber(EShaderParamSlot InSlot) noexcept;

        /** Where the shader reads the slot: "u_Params[0].x", "u_Colors[1]", "u_Textures[MATERIAL_SLOT_0]". */
        OpaaxString Expression(EShaderParamSlot InSlot);
    }
}
