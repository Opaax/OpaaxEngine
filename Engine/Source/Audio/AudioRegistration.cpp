// The Audio module's types. Each registers itself; see Engine/Registries/AutoRegistration.h.
#include "Audio/AudioClipResource.h"
#include "Audio/AudioSubsystem.h"
#include "Audio/Components/AudioListenerComponent.h"
#include "Audio/Components/AudioSourceComponent.h"
#include "Engine/Registries/AutoRegistration.h"

namespace Opaax
{
    OPAAX_REGISTER_NAMED_RESOURCE(AudioClipResource, "AudioClip");

    OPAAX_REGISTER_NAMED_COMPONENT(AudioSourceComponent, "AudioSource");
    OPAAX_REGISTER_NAMED_COMPONENT(AudioListenerComponent, "AudioListener");

    // After gameplay and physics, so sounds follow where the entities ended up this frame.
    OPAAX_REGISTER_NAMED_WORLD_SUBSYSTEM(AudioSubsystem, "Audio", WorldSubsystemOrder::Presentation);
}
