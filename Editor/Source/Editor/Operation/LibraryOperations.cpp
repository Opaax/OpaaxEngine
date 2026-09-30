#include "Editor/Operation/LibraryOperations.h"
#include "Editor/Operation/ResourceOperations.h"

#include "Core/String/OpaaxPathString.h"
#include "Editor/Resources/Types/Animation/EditorAnimationLibraryDocument.h"
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
         * Records a whole-list replacement under InLabel, and applies it to the document. The default is
         * checked here too: a default naming a missing clip would silently fall back to the first entry.
         */
        void RecordEntries(EditorContext& InContext, TDynArray<AnimationLibraryEntry> InAfter,
                           const char* InLabel, const OpaaxStringID InAfterDefault)
        {
            AnimationLibraryData& lData = InContext.LibraryDocument.GetMutableData();

            // Cleared rather than repointed (the author picks the new default; empty means the first entry).
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

        /** Overload for edits that do not move the default. */
        void RecordEntries(EditorContext& InContext, TDynArray<AnimationLibraryEntry> InAfter,
                           const char* InLabel)
        {
            const OpaaxStringID lDefault = InContext.LibraryDocument.GetData().DefaultClip;

            RecordEntries(InContext, Move(InAfter), InLabel, lDefault);
        }

        /** Whether an entry other than InSkip already has InName. */
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

        // Reverted, not just refused: the drawer already wrote the duplicate, and a duplicate name makes
        // one clip unreachable.
        if (lEntry.Name.IsValid() && NameTakenByOther(lData, lEntry.Name, InIndex))
        {
            OPAAX_LOG(LogEditorAnimationLibraryDocument, Warn,
                      "'{}' is already a clip name in this library — the edit was reverted",
                      lEntry.Name.CStr());

            lEntry = InBefore;
            return false;
        }

        // A clip dropped on an unnamed entry is named from the file stem (unless that name is taken).
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

        // A rename carries the default with it; otherwise the default names nothing and silently falls
        // back to the first entry.
        OpaaxStringID lDefault = lData.DefaultClip;

        if (lDefault.IsValid() && lDefault == InBefore.Name && lEntry.Name.IsValid())
        {
            lDefault = lEntry.Name;
        }

        // The list after the fix-ups, with InBefore restored as the undo target.
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

        // Reload the resource, so an entity already using this library sees the change.
        ResourceOps::SavedToDisk<AnimationLibraryResource>(InContext, InContext.LibraryDocument.AbsPath());

        return true;
    }
}
