// The Renderer module's types. Each registers itself; see Engine/Registries/AutoRegistration.h.
#include "Engine/Registries/AutoRegistration.h"
#include "Renderer/Camera/CameraComponent.h"
#include "Renderer/Components/EnvironmentComponent.h"
#include "Renderer/Components/Light2DComponent.h"
#include "Renderer/Components/ShadowCaster2DComponent.h"
#include "Renderer/Materials/Material2D.h"
#include "Renderer/Components/QuadComponent.h"
#include "Renderer/Components/SpriteComponent.h"
#include "Renderer/Components/TextComponent.h"
#include "Renderer/Text/FontFace.h"
#include "Renderer/Text/FontFamily.h"
#include "Renderer/Textures/SpriteSheet.h"
#include "Renderer/Textures/Texture2D.h"

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
    OPAAX_REGISTER_NAMED_COMPONENT(ShadowCaster2DComponent, "ShadowCaster2D");

    // How a sprite takes light, shared by the sprites that point at it.
    OPAAX_REGISTER_OBJECT(Material2D);

    // A typeface's faces by style, which text asks for instead of a .ttf.
    OPAAX_REGISTER_OBJECT(FontFamily);

    // A texture cut into frames, which sprites, UI images and clips show.
    OPAAX_REGISTER_OBJECT(SpriteSheet);

    // An image, read from its .png (or .jpg, .tga, .bmp) file.
    OPAAX_REGISTER_OBJECT(Texture2D);

    // A font's glyphs, baked from its .ttf (or .otf) file.
    OPAAX_REGISTER_OBJECT(FontFace);
}
