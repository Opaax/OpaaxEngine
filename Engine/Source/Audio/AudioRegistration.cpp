// The Audio module's types. Each registers itself; see Engine/Registries/AutoRegistration.h.
#include "Audio/AudioBank.h"
#include "Audio/AudioClipDef.h"
#include "Audio/AudioClipResource.h"
#include "Audio/AudioGlobalBank.h"
#include "Audio/AudioSubsystem.h"
#include "Audio/Components/AudioBanksComponent.h"
#include "Audio/Components/AudioListenerComponent.h"
#include "Audio/Components/AudioSourceComponent.h"
#include "Engine/Registries/AutoRegistration.h"

namespace Opaax
{
    OPAAX_REGISTER_NAMED_RESOURCE(AudioClipResource, "AudioClip");
    OPAAX_REGISTER_OBJECT(AudioClipDef);
    OPAAX_REGISTER_OBJECT(AudioBank);

    OPAAX_REGISTER_NAMED_COMPONENT(AudioSourceComponent, "AudioSource");
    OPAAX_REGISTER_NAMED_COMPONENT(AudioListenerComponent, "AudioListener");
    OPAAX_REGISTER_NAMED_COMPONENT(AudioBanksComponent, "AudioBanks");

    OPAAX_REGISTER_NAMED_GAME_INSTANCE_SUBSYSTEM(AudioGlobalBank, "AudioGlobalBank", 0);

    // After gameplay and physics, so sounds follow where the entities ended up this frame.
    OPAAX_REGISTER_NAMED_WORLD_SUBSYSTEM(AudioSubsystem, "Audio", WorldSubsystemOrder::Presentation);
}
