#include "Editor/Operation/LibraryOperations.h"

#include "Core/String/OpaaxPathString.h"
#include "Editor/EditorAnimationLibraryDocument.h"
#include "Editor/EditorContext.h"
#include "Editor/Undo/AnimationLibraryUndoables.h"
#include "Editor/Undo/EditorUndo.h"

#include "Engine/Subsystems/Resources/ResourceManager.h"
#include "Engine/Subsystems/Resources/Types/Animation/AnimationLibraryFile.h"
#include "Engine/Subsystems/Resources/Types/Animation/AnimationLibraryResource.h"

namespace Opaax::Editor
{
    namespace
    {
        /**
         * Record a whole-list replacement under InLabel, and publish it into the document.
         *
         * The default is carried in the same step and RE-VALIDATED here rather than at each call
         * site: every list edit can dangle it, and a default naming a clip the library no longer
         * has resolves to the FIRST entry — silently, and to a different clip than it says.
         */
        void RecordEntries(EditorContext& InContext, TDynArray<AnimationLibraryEntry> InAfter,
                           const char* InLabel, const OpaaxStringID InAfterDefault)
        {
            AnimationLibraryData& lData = InContext.LibraryDocument.GetMutableData();

            // Cleared rather than repointed: which clip should inherit the role is the author's
            // call, and the empty default already MEANS "the first entry".
            OpaaxStringID lDefault = InAfterDefault;

            if (lDefault.IsValid())
            {
                bool lStillThere = false;

                for (const AnimationLibraryEntry& lEntry : InAfter)
                {
                    if (lEntry.Name == lDefault) { lStillThere = true; break; }
                }

                if (!lStillThere)
                {
                    OPAAX_LOG(LogEditorAnimationLibraryDocument, Info,
                              "Default clip '{}' is no longer in this library — cleared to the first entry",
                              lDefault.CStr());
                    lDefault = OpaaxStringID();
                }
            }

            LibraryEntriesEdit lStep;
            lStep.LibraryPath   = InContext.LibraryDocument.AbsPath();
            lStep.Before        = lData.Entries;
            lStep.After         = InAfter;
            lStep.BeforeDefault = lData.DefaultClip;
            lStep.AfterDefault  = lDefault;
            lStep.LabelText     = InLabel;

            lData.Entries     = Move(InAfter);
            lData.DefaultClip = lDefault;

            InContext.Undo.Record(Move(lStep));
        }

        /** The overload every edit that does not MOVE the default uses. */
        void RecordEntries(EditorContext& InContext, TDynArray<AnimationLibraryEntry> InAfter,
                           const char* InLabel)
        {
            const OpaaxStringID lDefault = InContext.LibraryDocument.GetData().DefaultClip;

            RecordEntries(InContext, Move(InAfter), InLabel, lDefault);
        }

        /** Whether any entry OTHER than InSkip already answers to InName. */
        bool NameTakenByOther(const AnimationLibraryData& InData, const OpaaxStringID InName,
                              const Uint32 InSkip)
        {
            for (Uint32 lIndex = 0; lIndex < InData.EntryCount(); ++lIndex)
            {
                if (lIndex != InSkip && InData.Entries[lIndex].Name == InName) { return true; }
            }

            return false;
        }
    }

    bool LibraryOps::AddEntry(EditorContext& InContext)
    {
        if (!InContext.LibraryDocument.IsOpen()) { return false; }

        TDynArray<AnimationLibraryEntry> lAfter = InContext.LibraryDocument.GetData().Entries;
        lAfter.emplace_back(AnimationLibraryEntry{});

        RecordEntries(InContext, Move(lAfter), "Add Clip");
        return true;
    }

    bool LibraryOps::RemoveEntry(EditorContext& InContext, const Uint32 InIndex)
    {
        if (!InContext.LibraryDocument.IsOpen()) { return false; }

        const AnimationLibraryData& lData = InContext.LibraryDocument.GetData();

        if (InIndex >= lData.EntryCount()) { return false; }

        TDynArray<AnimationLibraryEntry> lAfter = lData.Entries;
        lAfter.erase(lAfter.begin() + InIndex);

        RecordEntries(InContext, Move(lAfter), "Remove Clip");
        return true;
    }

    bool LibraryOps::MoveEntry(EditorContext& InContext, const Uint32 InIndex, const Int32 InDelta)
    {
        if (!InContext.LibraryDocument.IsOpen()) { return false; }

        const AnimationLibraryData& lData = InContext.LibraryDocument.GetData();

        if (InIndex >= lData.EntryCount()) { return false; }

        const Int32 lRaw    = static_cast<Int32>(InIndex) + InDelta;
        const Int32 lLast   = static_cast<Int32>(lData.EntryCount()) - 1;
        const Int32 lTarget = (lRaw < 0) ? 0 : ((lRaw > lLast) ? lLast : lRaw);

        if (lTarget == static_cast<Int32>(InIndex)) { return false; }

        TDynArray<AnimationLibraryEntry> lAfter = lData.Entries;
        const AnimationLibraryEntry      lMoved = lAfter[InIndex];

        lAfter.erase(lAfter.begin() + InIndex);
        lAfter.insert(lAfter.begin() + lTarget, lMoved);

        RecordEntries(InContext, Move(lAfter), "Move Clip");
        return true;
    }

    bool LibraryOps::CommitEntryEdit(EditorContext& InContext, const Uint32 InIndex,
                                     const AnimationLibraryEntry& InBefore)
    {
        if (!InContext.LibraryDocument.IsOpen()) { return false; }

        AnimationLibraryData& lData = InContext.LibraryDocument.GetMutableData();

        if (InIndex >= lData.EntryCount()) { return false; }

        AnimationLibraryEntry& lEntry = lData.Entries[InIndex];

        // REVERTED, not merely refused: the drawer already wrote the duplicate into the document,
        // so leaving it would ship a library where one clip can never be reached.
        if (lEntry.Name.IsValid() && NameTakenByOther(lData, lEntry.Name, InIndex))
        {
            OPAAX_LOG(LogEditorAnimationLibraryDocument, Warn,
                      "'{}' is already a clip name in this library — the edit was reverted",
                      lEntry.Name.CStr());

            lEntry = InBefore;
            return false;
        }

        // A clip arrived on an unnamed entry: name it from the file stem so dropping one in is ONE
        // action. Skipped when that name is taken, for the reason above.
        if (!lEntry.Name.IsValid() && !lEntry.Clip.IsEmpty())
        {
            const OpaaxStringView lStem = PathString::Stem(lEntry.Clip.Path);

            if (!lStem.IsEmpty())
            {
                const OpaaxStringID lCandidate(lStem.ToString());

                if (!NameTakenByOther(lData, lCandidate, InIndex))
                {
                    lEntry.Name = lCandidate;
                }
            }
        }

        if (lEntry.Name == InBefore.Name && lEntry.Clip.Path == InBefore.Clip.Path)
        {
            return false;   // a gesture that changed nothing is not a step
        }

        // A RENAME CARRIES THE DEFAULT WITH IT. Without this, renaming the default entry leaves the
        // default naming something that no longer exists — which does not fail, it falls through to
        // the FIRST entry, so the library silently plays a different clip than it says it does.
        // `SheetOps::Slice` clamps its DefaultFrame in the same step for exactly this reason.
        OpaaxStringID lDefault = lData.DefaultClip;

        if (lDefault.IsValid() && lDefault == InBefore.Name && lEntry.Name.IsValid())
        {
            lDefault = lEntry.Name;
        }

        // The list AFTER the fix-ups, with InBefore put back in place as the undo target.
        TDynArray<AnimationLibraryEntry> lAfter  = lData.Entries;
        TDynArray<AnimationLibraryEntry> lBefore = lData.Entries;
        lBefore[InIndex] = InBefore;

        LibraryEntriesEdit lStep;
        lStep.LibraryPath   = InContext.LibraryDocument.AbsPath();
        lStep.Before        = Move(lBefore);
        lStep.After         = Move(lAfter);
        lStep.BeforeDefault = lData.DefaultClip;
        lStep.AfterDefault  = lDefault;
        lStep.LabelText     = "Edit Clip";

        lData.DefaultClip = lDefault;

        InContext.Undo.Record(Move(lStep));
        return true;
    }

    bool LibraryOps::SetDefaultClip(EditorContext& InContext, const OpaaxStringID InName)
    {
        if (!InContext.LibraryDocument.IsOpen()) { return false; }

        AnimationLibraryData& lData = InContext.LibraryDocument.GetMutableData();

        if (lData.DefaultClip == InName) { return false; }

        LibraryDefaultClip lStep;
        lStep.LibraryPath = InContext.LibraryDocument.AbsPath();
        lStep.Before      = lData.DefaultClip;
        lStep.After       = InName;

        lData.DefaultClip = InName;

        InContext.Undo.Record(Move(lStep));
        return true;
    }

    bool LibraryOps::Save(EditorContext& InContext)
    {
        if (!InContext.LibraryDocument.IsOpen()) { return false; }

        if (!AnimationLibraryFile::Save(InContext.LibraryDocument.AbsPath(),
                                        InContext.LibraryDocument.GetData()))
        {
            return false;   // AnimationLibraryFile logged why
        }

        InContext.LibraryDocument.MarkSaved();

        // AND PUBLISH IT, for ClipOps::Save's reason ([[L75]]): without this an entity already
        // holding this library keeps resolving names against the first parse.
        InContext.Resources.Reload<AnimationLibraryResource>(InContext.LibraryDocument.AbsPath().CStr());

        return true;
    }
}
