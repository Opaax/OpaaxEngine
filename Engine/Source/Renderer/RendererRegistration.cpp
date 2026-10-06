// The Renderer module's types. Each registers itself; see Engine/Registries/AutoRegistration.h.
#include "Engine/Registries/AutoRegistration.h"
#include "Renderer/Camera/CameraComponent.h"
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

    OPAAX_REGISTER_NAMED_RESOURCE(TextureResource, "Texture");
    OPAAX_REGISTER_NAMED_RESOURCE(SpriteSheetResource, "SpriteSheet");
    OPAAX_REGISTER_NAMED_RESOURCE(FontFaceResource, "FontFace");
    OPAAX_REGISTER_NAMED_RESOURCE(FontFamilyResource, "FontFamily");
}
