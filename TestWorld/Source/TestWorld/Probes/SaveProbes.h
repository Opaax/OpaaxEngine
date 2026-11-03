#pragma once

#include "Object/ObjectPtr.h"
#include "Save/SaveGame.h"
#include "Save/SaveGameSubsystem.h"
#include "World/Behaviour/Behaviour.h"

namespace TestWorld
{
    // =============================================================================
    // ProbeSave — the TestWorld game's save: what the Saves level keeps between launches.
    // =============================================================================
    class ProbeSave final : public Opaax::SaveGame
    {
        OPAAX_OBJECT(ProbeSave, Opaax::SaveGame)

    public:
        Opaax::Int32       Runs = 0;
        Opaax::OpaaxString LastLevel;

        OPAAX_PROPERTIES(ProbeSave, OPAAX_PROP(Runs), OPAAX_PROP(LastLevel))
    };

    // =============================================================================
    // SaveProbe — loads its Slot in the background when it starts, counts one more run and saves it
    //   back in the background, as a game keeps its progress. Only a slot without a save starts a new
    //   one: a slot it cannot read is left as it is. Each answer goes in its fields, where the test
    //   reads them.
    // =============================================================================
    class SaveProbe : public Opaax::Behaviour
    {
        OPAAX_OBJECT(SaveProbe, Opaax::Behaviour)

    public:
        Opaax::OpaaxString Slot;

        /** How the load ended ("Ok", "Missing"...); empty until it has. */
        Opaax::OpaaxString LoadAnswer;
        Opaax::Int32       LoadedRuns = -1;

        /** How the save ended; empty until it has, or when nothing was saved. */
        Opaax::OpaaxString SaveAnswer;

        /** Set when an answer came inside the call that asked for it: it must come at a later Update. */
        bool bAnsweredInCall = false;

        OPAAX_PROPERTIES(SaveProbe, OPAAX_PROP(Slot), OPAAX_PROP(LoadAnswer), OPAAX_PROP(LoadedRuns),
                         OPAAX_PROP(SaveAnswer), OPAAX_PROP(bAnsweredInCall))

        void OnStart() override
        {
            Opaax::SaveGameSubsystem* lSaves = GetGameSubsystem<Opaax::SaveGameSubsystem>();
            if (lSaves == nullptr)
            {
                LoadAnswer = "NoSaveSubsystem";
                return;
            }

            // The answer comes frames later: this probe may be gone by then.
            const Opaax::TWeakObjectPtr<SaveProbe> lSelf(this);

            m_bAsking = true;
            lSaves->LoadAsync<ProbeSave>(Slot, [lSelf, lSaves](const Opaax::ESaveResult InResult,
                                                               Opaax::TStrongObjectPtr<ProbeSave> InSave)
            {
                if (SaveProbe* lProbe = lSelf.Get())
                {
                    lProbe->OnLoaded(*lSaves, InResult, Opaax::Move(InSave));
                }
            });
            m_bAsking = false;
        }

    private:
        void OnLoaded(Opaax::SaveGameSubsystem& InSaves, const Opaax::ESaveResult InResult,
                      Opaax::TStrongObjectPtr<ProbeSave> InSave)
        {
            bAnsweredInCall = bAnsweredInCall || m_bAsking;
            LoadAnswer      = Opaax::ToString(InResult);
            LoadedRuns      = InSave.IsValid() ? InSave->Runs : -1;

            if (InResult == Opaax::ESaveResult::Missing)
            {
                InSave = Opaax::NewObject<ProbeSave>();
            }
            else if (InResult != Opaax::ESaveResult::Ok)
            {
                return;
            }

            ++InSave->Runs;
            InSave->LastLevel = "Saves";

            const Opaax::TWeakObjectPtr<SaveProbe> lSelf(this);

            m_bAsking = true;
            InSaves.SaveAsync(Slot, *InSave, [lSelf](const Opaax::ESaveResult InSaved)
            {
                if (SaveProbe* lProbe = lSelf.Get())
                {
                    lProbe->bAnsweredInCall = lProbe->bAnsweredInCall || lProbe->m_bAsking;
                    lProbe->SaveAnswer      = Opaax::ToString(InSaved);
                }
            });
            m_bAsking = false;
        }

        // True while this probe is inside a call that asks the save subsystem.
        bool m_bAsking = false;
    };
}
