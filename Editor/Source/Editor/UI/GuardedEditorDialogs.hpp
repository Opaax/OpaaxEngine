#pragma once

#include <algorithm>
#include <string>

#include "Core/OpaaxTypes.h"            // TUniquePtr, TDynArray, Move
#include "Core/String/OpaaxString.hpp"
#include "Editor/UI/IEditorDialogs.h"

namespace Opaax::Editor
{
    // =============================================================================
    // GuardedEditorDialogs — the editor's dialogs, which stay closed while they are unattended (an
    //   automation request runs: nobody is there to answer, and an OS dialog would hold the editor
    //   until someone did). Unattended, a question is answered No and a file picker is cancelled,
    //   and what was asked is kept so the request can say why it did nothing.
    // =============================================================================
    class GuardedEditorDialogs final : public IEditorDialogs
    {
        // =============================================================================
        // CTORS - DTORS
        // =============================================================================
    public:
        /** @param InDialogs The dialogs shown when attended */
        explicit GuardedEditorDialogs(TUniquePtr<IEditorDialogs> InDialogs) : m_Dialogs(Move(InDialogs)) {}

        // =============================================================================
        // Functions
        // =============================================================================
    public:
        /** While unattended, no dialog opens. */
        void SetUnattended(const bool bInUnattended) noexcept { m_bUnattended = bInUnattended; }
        bool IsUnattended() const noexcept { return m_bUnattended; }

        /**
         * What was asked while unattended ("Title: message", or "Title" for a file picker), oldest
         * first; then forgets it.
         */
        TDynArray<OpaaxString> TakeUnanswered()
        {
            TDynArray<OpaaxString> lAsked = Move(m_Unanswered);
            m_Unanswered.clear();
            return lAsked;
        }

        // =============================================================================
        // Override
        // =============================================================================
    public:
        //~Begin IEditorDialogs interface
        void OpenFile(const FileDialogRequest& InRequest, FPathChosen InOnChosen) override
        {
            if (m_bUnattended) { m_Unanswered.push_back(InRequest.Title); return; }
            m_Dialogs->OpenFile(InRequest, Move(InOnChosen));
        }

        void SaveFile(const FileDialogRequest& InRequest, FPathChosen InOnChosen) override
        {
            if (m_bUnattended) { m_Unanswered.push_back(InRequest.Title); return; }
            m_Dialogs->SaveFile(InRequest, Move(InOnChosen));
        }

        void PickFolder(const FileDialogRequest& InRequest, FPathChosen InOnChosen) override
        {
            if (m_bUnattended) { m_Unanswered.push_back(InRequest.Title); return; }
            m_Dialogs->PickFolder(InRequest, Move(InOnChosen));
        }

        void Confirm(const OpaaxString& InTitle, const OpaaxString& InMessage, FAnswered InOnAnswered) override
        {
            if (!m_bUnattended)
            {
                m_Dialogs->Confirm(InTitle, InMessage, Move(InOnAnswered));
                return;
            }

            // On one line: it ends up in a request's answer.
            std::string lAsked = std::string(InTitle.CStr()) + ": " + InMessage.CStr();
            std::replace(lAsked.begin(), lAsked.end(), '\n', ' ');
            m_Unanswered.emplace_back(lAsked.c_str());

            // The safe answer: the question guards something that cannot be undone.
            if (InOnAnswered)
            {
                InOnAnswered(EDialogAnswer::No);
            }
        }
        //~End IEditorDialogs interface

        // =============================================================================
        // Members
        // =============================================================================
    private:
        TUniquePtr<IEditorDialogs> m_Dialogs;
        TDynArray<OpaaxString>     m_Unanswered;
        bool                       m_bUnattended = false;
    };
}
