#include "Editor/Assets/Editors/AudioClipEditor.h"

#include <imgui.h>

#include "Application/Services/IEngine.h"
#include "Audio/AudioClip.h"
#include "Audio/AudioManager.h"
#include "Editor/Assets/AssetDocument.hpp"
#include "Editor/EditorContext.h"

namespace Opaax::Editor
{
    AudioClipEditor::~AudioClipEditor()
    {
        if (m_Sound.IsValid())
        {
            m_Context.Engine.GetAudio().Stop(m_Sound);
        }
    }

    void AudioClipEditor::DrawContents(AssetDocument& InDocument)
    {
        const AudioClip* lClip = Cast<AudioClip>(InDocument.GetAsset());
        if (lClip == nullptr)
        {
            return;
        }

        AudioManager& lAudio = m_Context.Engine.GetAudio();

        // The voice shares the clip's data, so the window may close while it plays: the destructor stops it.
        if (m_Sound.IsValid() && lAudio.IsPlaying(m_Sound))
        {
            if (ImGui::Button("Stop"))
            {
                lAudio.Stop(m_Sound);
                m_Sound = SoundHandle{};
            }
        }
        else
        {
            ImGui::BeginDisabled(lClip->IsSilent());
            if (ImGui::Button("Play"))
            {
                m_Sound = lAudio.Play(*lClip);
            }
            ImGui::EndDisabled();
        }

        ImGui::TextDisabled("%.2f s, %u channel(s), %u Hz", lClip->GetDuration(), lClip->GetChannels(),
                            lClip->GetSampleRate());
        ImGui::TextDisabled(lClip->IsStreamed() ? "streamed: decoded while it plays (%llu KB)"
                                                : "decoded when it loads (%llu KB)",
                            static_cast<unsigned long long>(lClip->GetResourceSize() / 1024u));
    }
}
