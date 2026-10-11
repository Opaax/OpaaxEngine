#pragma once

#if OPAAX_WITH_EDITOR

namespace Opaax::Editor
{
    /**
     * @enum EEditorState
     * Play-In-Editor state machine.
     *  Editing — scene is editable; play-only subsystems are skipped.
     *  Playing — gameplay running; all subsystems tick.
     *  Paused  — gameplay frozen; render still runs so the user sees the last frame.
     */
    enum class EEditorState : unsigned char
    {
        Editing,
        Playing,
        Paused
    };

    inline const char* EditorStateToString(EEditorState State) noexcept
    {
        switch (State)
        {
            case EEditorState::Editing: return "Editing";
            case EEditorState::Playing: return "Playing";
            case EEditorState::Paused:  return "Paused";
        }
        return "Unknown";
    }

} // namespace Opaax::Editor

#endif // OPAAX_WITH_EDITOR
