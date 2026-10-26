// The Audio module's types. Each registers itself; see Engine/Registries/AutoRegistration.h.
#include "Audio/AudioBank.h"
#include "Audio/AudioClip.h"
#include "Audio/AudioClipDef.h"
#include "Audio/AudioGlobalBank.h"
#include "Audio/AudioSubsystem.h"
#include "Audio/Components/AudioBanksComponent.h"
#include "Audio/Components/AudioListenerComponent.h"
#include "Audio/Components/AudioSourceComponent.h"
#include "Engine/Registries/AutoRegistration.h"

namespace Opaax
{
    // A sound, read from its .wav (or .mp3, .flac) file.
    OPAAX_REGISTER_OBJECT(AudioClip);
    OPAAX_REGISTER_OBJECT(AudioClipDef);
    OPAAX_REGISTER_OBJECT(AudioBank);

    OPAAX_REGISTER_NAMED_COMPONENT(AudioSourceComponent, "AudioSource");
    OPAAX_REGISTER_NAMED_COMPONENT(AudioListenerComponent, "AudioListener");
    OPAAX_REGISTER_NAMED_COMPONENT(AudioBanksComponent, "AudioBanks");

    OPAAX_REGISTER_OBJECT(AudioGlobalBank);

    // After gameplay and physics, so sounds follow where the entities ended up this frame.
    OPAAX_REGISTER_OBJECT(AudioSubsystem);
}
