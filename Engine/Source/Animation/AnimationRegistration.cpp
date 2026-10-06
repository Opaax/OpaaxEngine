// The Animation module's types. Each registers itself; see Engine/Registries/AutoRegistration.h.
#include "Animation/AnimationClipResource.h"
#include "Animation/AnimationLibraryResource.h"
#include "Animation/SpriteAnimationSubsystem.h"
#include "Animation/SpriteAnimatorComponent.h"
#include "Engine/Registries/AutoRegistration.h"
#include "Renderer/Textures/SpriteSheetResource.h"   // completes the clip's sheet reference

namespace Opaax
{
    OPAAX_REGISTER_NAMED_COMPONENT(SpriteAnimatorComponent, "SpriteAnimator");

    OPAAX_REGISTER_NAMED_RESOURCE(AnimationClipResource, "AnimationClip");
    OPAAX_REGISTER_NAMED_RESOURCE(AnimationLibraryResource, "AnimationLibrary");

    // Play worlds only (its ShouldCreate decides). Reads the frame's final state.
    OPAAX_REGISTER_NAMED_WORLD_SUBSYSTEM(SpriteAnimationSubsystem, "SpriteAnimation", WorldSubsystemOrder::Presentation);
}
