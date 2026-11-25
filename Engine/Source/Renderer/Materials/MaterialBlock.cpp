#include "Renderer/Materials/MaterialBlock.h"

#include "Renderer/Materials/Material2D.h"

namespace Opaax
{
    MaterialBlock MakeMaterialBlock(const Material2D& InMaterial, const double InWorldTime, const double InAppTime) noexcept
    {
        MaterialBlock lBlock;
        lBlock.Time      = Vector4F{ static_cast<float>(InWorldTime), static_cast<float>(InAppTime), 0.f, 0.f };
        lBlock.Params[0] = Vector4F{ InMaterial.Param0, InMaterial.Param1, InMaterial.Param2, InMaterial.Param3 };
        lBlock.Params[1] = Vector4F{ InMaterial.Param4, InMaterial.Param5, InMaterial.Param6, InMaterial.Param7 };
        lBlock.Colors[0] = Vector4F{ InMaterial.Color0.r, InMaterial.Color0.g, InMaterial.Color0.b, InMaterial.Color0.a };
        lBlock.Colors[1] = Vector4F{ InMaterial.Color1.r, InMaterial.Color1.g, InMaterial.Color1.b, InMaterial.Color1.a };
        return lBlock;
    }
}
