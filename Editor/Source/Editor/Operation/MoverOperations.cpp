#include "Editor/Operation/MoverOperations.h"
#include "Editor/Operation/ResourceOperations.h"

#include "Core/String/OpaaxPathString.h"
#include "Editor/EditorContext.h"
#include "Editor/Resources/Types/Mover/EditorMoveModeDocument.h"
#include "Editor/Resources/Types/Mover/EditorMoverDocument.h"
#include "Editor/Undo/EditorUndo.h"
#include "Editor/Undo/MoverUndoables.h"

#include "Resources/ResourceManager.h"
#include "Movement/Assets/MoveModeFile.h"
#include "Movement/Assets/MoveModeResource.h"
#include "Movement/Assets/MoverFile.h"
#include "Movement/Assets/MoverResource.h"

namespace Opaax::Editor
{
    namespace
    {
        /**
         * Records a whole-list replacement under InLabel, and applies it to the document. The default is
         * checked here too: a default naming a missing mode would silently fall back to the first entry.
         */
        void RecordEntries(EditorContext& InContext, TDynArray<MoverEntry> InAfter,
                           const char* InLabel, const OpaaxStringID InAfterDefault)
        {
            MoverData& lData = InContext.MoverDocument.GetMutableData();

            // Cleared rather than repointed (the author picks the new default; empty means the first entry).
            OpaaxStringID lDefault = InAfterDefault;

            if (lDefault.IsValid())
            {
                bool lStillThere = false;

                for (const MoverEntry& lEntry : InAfter)
                {
                    if (lEntry.Name == lDefault) { lStillThere = true; break; }
                }

                if (!lStillThere)
                {
                    OPAAX_LOG(LogEditorMoverDocument, Info,
                              "Default mode '{}' is no longer in this mover — cleared to the first entry",
                              lDefault.CStr());
                    lDefault = OpaaxStringID();
                }
            }

            MoverEntriesEdit lStep;
            lStep.MoverPath     = InContext.MoverDocument.AbsPath();
            lStep.Before        = lData.Entries;
            lStep.After         = InAfter;
            lStep.BeforeDefault = lData.DefaultMode;
            lStep.AfterDefault  = lDefault;
            lStep.LabelText     = InLabel;

            lData.Entries     = Move(InAfter);
            lData.DefaultMode = lDefault;

            InContext.Undo.Record(Move(lStep));
        }

        /** Overload for edits that do not move the default. */
        void RecordEntries(EditorContext& InContext, TDynArray<MoverEntry> InAfter, const char* InLabel)
        {
            const OpaaxStringID lDefault = InContext.MoverDocument.GetData().DefaultMode;

            RecordEntries(InContext, Move(InAfter), InLabel, lDefault);
        }

        /** Whether an entry other than InSkip already has InName. */
        bool NameTakenByOther(const MoverData& InData, const OpaaxStringID InName, const Uint32 InSkip)
        {
            for (Uint32 lIndex = 0; lIndex < InData.EntryCount(); ++lIndex)
            {
                if (lIndex != InSkip && InData.Entries[lIndex].Name == InName) { return true; }
            }

            return false;
        }
    }

    // =========================================================================
    // MoverOps
    // =========================================================================
    bool MoverOps::AddEntry(EditorContext& InContext)
    {
        if (!InContext.MoverDocument.IsOpen()) { return false; }

        TDynArray<MoverEntry> lAfter = InContext.MoverDocument.GetData().Entries;
        lAfter.emplace_back(MoverEntry{});

        RecordEntries(InContext, Move(lAfter), "Add Mode");
        return true;
    }

    bool MoverOps::RemoveEntry(EditorContext& InContext, const Uint32 InIndex)
    {
        if (!InContext.MoverDocument.IsOpen()) { return false; }

        const MoverData& lData = InContext.MoverDocument.GetData();

        if (InIndex >= lData.EntryCount()) { return false; }

        TDynArray<MoverEntry> lAfter = lData.Entries;
        lAfter.erase(lAfter.begin() + InIndex);

        RecordEntries(InContext, Move(lAfter), "Remove Mode");
        return true;
    }

    bool MoverOps::MoveEntry(EditorContext& InContext, const Uint32 InIndex, const Int32 InDelta)
    {
        if (!InContext.MoverDocument.IsOpen()) { return false; }

        const MoverData& lData = InContext.MoverDocument.GetData();

        if (InIndex >= lData.EntryCount()) { return false; }

        const Int32 lRaw    = static_cast<Int32>(InIndex) + InDelta;
        const Int32 lLast   = static_cast<Int32>(lData.EntryCount()) - 1;
        const Int32 lTarget = (lRaw < 0) ? 0 : ((lRaw > lLast) ? lLast : lRaw);

        if (lTarget == static_cast<Int32>(InIndex)) { return false; }

        TDynArray<MoverEntry> lAfter = lData.Entries;
        const MoverEntry      lMoved = lAfter[InIndex];

        lAfter.erase(lAfter.begin() + InIndex);
        lAfter.insert(lAfter.begin() + lTarget, lMoved);

        RecordEntries(InContext, Move(lAfter), "Move Mode");
        return true;
    }

    bool MoverOps::CommitEntryEdit(EditorContext& InContext, const Uint32 InIndex,
                                   const MoverEntry& InBefore)
    {
        if (!InContext.MoverDocument.IsOpen()) { return false; }

        MoverData& lData = InContext.MoverDocument.GetMutableData();

        if (InIndex >= lData.EntryCount()) { return false; }

        MoverEntry& lEntry = lData.Entries[InIndex];

        // Reverted, not just refused: the drawer already wrote the duplicate, and a duplicate name makes
        // one mode unreachable.
        if (lEntry.Name.IsValid() && NameTakenByOther(lData, lEntry.Name, InIndex))
        {
            OPAAX_LOG(LogEditorMoverDocument, Warn,
                      "'{}' is already a mode name in this mover — the edit was reverted",
                      lEntry.Name.CStr());

            lEntry = InBefore;
            return false;
        }

        // A tuning dropped on an unnamed entry is named from the file stem (unless that name is taken).
        if (!lEntry.Name.IsValid() && !lEntry.ModeAsset.IsEmpty())
        {
            const OpaaxStringView lStem = PathString::Stem(lEntry.ModeAsset.Path);

            if (!lStem.IsEmpty())
            {
                const OpaaxStringID lCandidate(lStem.ToString());

                if (!NameTakenByOther(lData, lCandidate, InIndex))
                {
                    lEntry.Name = lCandidate;
                }
            }
        }

        if (lEntry.Name == InBefore.Name && lEntry.ModeAsset.Path == InBefore.ModeAsset.Path)
        {
            return false;   // a gesture that changed nothing is not a step
        }

        // A rename carries the default with it; otherwise the default names nothing and silently falls
        // back to the first entry.
        OpaaxStringID lDefault = lData.DefaultMode;

        if (lDefault.IsValid() && lDefault == InBefore.Name && lEntry.Name.IsValid())
        {
            lDefault = lEntry.Name;
        }

        // The list after the fix-ups, with InBefore restored as the undo target.
        TDynArray<MoverEntry> lAfter  = lData.Entries;
        TDynArray<MoverEntry> lBefore = lData.Entries;
        lBefore[InIndex] = InBefore;

        MoverEntriesEdit lStep;
        lStep.MoverPath     = InContext.MoverDocument.AbsPath();
        lStep.Before        = Move(lBefore);
        lStep.After         = Move(lAfter);
        lStep.BeforeDefault = lData.DefaultMode;
        lStep.AfterDefault  = lDefault;
        lStep.LabelText     = "Edit Mode";

        lData.DefaultMode = lDefault;

        InContext.Undo.Record(Move(lStep));
        return true;
    }

    bool MoverOps::SetDefaultMode(EditorContext& InContext, const OpaaxStringID InName)
    {
        if (!InContext.MoverDocument.IsOpen()) { return false; }

        MoverData& lData = InContext.MoverDocument.GetMutableData();

        if (lData.DefaultMode == InName) { return false; }

        MoverDefaultMode lStep;
        lStep.MoverPath = InContext.MoverDocument.AbsPath();
        lStep.Before    = lData.DefaultMode;
        lStep.After     = InName;

        lData.DefaultMode = InName;

        InContext.Undo.Record(Move(lStep));
        return true;
    }

    bool MoverOps::Save(EditorContext& InContext)
    {
        if (!InContext.MoverDocument.IsOpen()) { return false; }

        if (!MoverFile::Save(InContext.MoverDocument.AbsPath(), InContext.MoverDocument.GetData()))
        {
            return false;   // MoverFile logged why
        }

        InContext.MoverDocument.MarkSaved();

        // Reload the resource, so an entity already using this mover sees the change.
        ResourceOps::SavedToDisk<MoverResource>(InContext, InContext.MoverDocument.AbsPath());

        return true;
    }

    // =========================================================================
    // MoveModeOps
    // =========================================================================
    bool MoveModeOps::CommitEdit(EditorContext& InContext, const MoveModeData& InBefore)
    {
        if (!InContext.MoveModeDocument.IsOpen()) { return false; }

        const MoveModeData& lAfter = InContext.MoveModeDocument.GetData();

        // Compared through the file serializer: one definition of "different" (the same one IsDirty uses).
        if (MoveModeFile::Serialize(lAfter) == MoveModeFile::Serialize(InBefore))
        {
            return false;   // a gesture that changed nothing is not a step
        }

        MoveModeEdit lStep;
        lStep.ModePath = InContext.MoveModeDocument.AbsPath();
        lStep.Before   = InBefore;
        lStep.After    = lAfter;

        InContext.Undo.Record(Move(lStep));
        return true;
    }

    bool MoveModeOps::Save(EditorContext& InContext)
    {
        if (!InContext.MoveModeDocument.IsOpen()) { return false; }

        if (!MoveModeFile::Save(InContext.MoveModeDocument.AbsPath(),
                                InContext.MoveModeDocument.GetData()))
        {
            return false;   // MoveModeFile logged why
        }

        InContext.MoveModeDocument.MarkSaved();

        ResourceOps::SavedToDisk<MoveModeResource>(InContext, InContext.MoveModeDocument.AbsPath());

        return true;
    }
}
