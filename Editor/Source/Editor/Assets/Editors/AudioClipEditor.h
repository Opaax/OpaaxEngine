#pragma once

#include "Audio/AudioTypes.h"         // SoundHandle
#include "Core/Maths/MathTypes.h"     // Vector2F
#include "Editor/Assets/IAssetEditor.h"

namespace Opaax::Editor
{
    struct EditorContext;

    // =============================================================================
    // AudioClipEditor — a sound file's window: Play and Stop to hear it, its length, channels and
    //   sample rate, and how it is kept (decoded when it loads, or streamed while it plays). Nothing to
    //   edit: the sound file is the source.
    // =============================================================================
    class AudioClipEditor final : public IAssetEditor
    {
    public:
        explicit AudioClipEditor(EditorContext& InContext) : m_Context(InContext) {}

        /** Stops the sound it started, if it still plays. */
        ~AudioClipEditor() override;

        //~Begin IAssetEditor interface
        void DrawContents(AssetDocument& InDocument) override;

        Vector2F GetDefaultSize() const override { return { 360.f, 200.f }; }
        //~End IAssetEditor interface

    private:
        EditorContext& m_Context;

        /** The sound Play started; invalid when none. */
        SoundHandle m_Sound;
    };
}
