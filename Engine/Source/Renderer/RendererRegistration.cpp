// The Renderer module's types. Each registers itself; see Engine/Registries/AutoRegistration.h.
#include "Engine/Registries/AutoRegistration.h"
#include "Renderer/Camera/CameraComponent.h"
#include "Renderer/Components/EnvironmentComponent.h"
#include "Renderer/Components/Light2DComponent.h"
#include "Renderer/Materials/Material2D.h"
#include "Renderer/Components/QuadComponent.h"
#include "Renderer/Components/SpriteComponent.h"
#include "Renderer/Components/TextComponent.h"
#include "Renderer/Text/FontFaceResource.h"
#include "Renderer/Text/FontFamilyResource.h"
#include "Renderer/Textures/SpriteSheetResource.h"
#include "Renderer/Textures/TextureResource.h"

namespace Opaax
{
    OPAAX_REGISTER_NAMED_COMPONENT(QuadComponent, "Quad");

    // Its old name, still read from older maps.
    OPAAX_REGISTER_COMPONENT_ALIAS("Dummy", "Quad");

    OPAAX_REGISTER_NAMED_COMPONENT(SpriteComponent, "Sprite");
    OPAAX_REGISTER_NAMED_COMPONENT(CameraComponent, "Camera");
    OPAAX_REGISTER_NAMED_COMPONENT(TextComponent, "Text");

    // How a world's picture is made (HDR, ambient, exposure, tonemap): one per level.
    OPAAX_REGISTER_NAMED_COMPONENT(EnvironmentComponent, "Environment");
    OPAAX_REGISTER_NAMED_COMPONENT(Light2DComponent, "Light2D");

    // How a sprite takes light, shared by the sprites that point at it.
    OPAAX_REGISTER_DATA_ASSET(Material2D);

    OPAAX_REGISTER_NAMED_RESOURCE(TextureResource, "Texture");
    OPAAX_REGISTER_NAMED_RESOURCE(SpriteSheetResource, "SpriteSheet");
    OPAAX_REGISTER_NAMED_RESOURCE(FontFaceResource, "FontFace");
    OPAAX_REGISTER_NAMED_RESOURCE(FontFamilyResource, "FontFamily");
}
