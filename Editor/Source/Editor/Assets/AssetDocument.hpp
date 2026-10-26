#pragma once

#include <nlohmann/json.hpp>

#include "Core/IO/FileIO.h"
#include "Core/Log/Logger.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxPathString.h"
#include "Core/String/OpaaxString.hpp"
#include "Core/String/OpaaxStringID.hpp"
#include "Object/ObjectClassRegistry.h"
#include "Object/ObjectPtr.h"
#include "Resources/Asset/Asset.h"
#include "Resources/Asset/AssetFile.h"
#include "Resources/ResourceFormat.h"   // NormalizeExtension

namespace Opaax::Editor
{
    OPAAX_LOG_CATEGORY(AssetDocument);

    // =============================================================================
    // AssetDocument — one open asset file: its own copy of the asset (the ResourceManager's copy is
    //   what a running game reads), the text last written (for the dirty mark), and, for a class no
    //   module registered, the raw properties (shown, never rewritten). A file of a class's own format
    //   (a .png) opens read-only: its file is the source, never rewritten. Header-only, so the tests
    //   drive it without the editor.
    // =============================================================================
    class AssetDocument
    {
        // =============================================================================
        // Making one
        // =============================================================================
    public:
        /**
         * Reads InAbsPath.
         * @param OutError Why it failed
         * @return Null when the file cannot be read, or a value does not fit its class
         */
        static TUniquePtr<AssetDocument> Open(const OpaaxString& InAbsPath, const ObjectClassRegistry& InClasses,
                                              OpaaxString& OutError)
        {
            const OpaaxStringID lExtension = NormalizeExtension(PathString::Extension(InAbsPath));
            if (lExtension != NormalizeExtension(AssetFile::ASSET_EXTENSION))
            {
                return OpenSource(InAbsPath, lExtension, InClasses, OutError);
            }

            AssetFile::Contents lContents;
            if (!AssetFile::Load(InAbsPath, lContents))
            {
                OutError = OpaaxString::Format("'{}' is not a readable asset file (see the log)", InAbsPath.CStr());
                return nullptr;
            }

            TUniquePtr<AssetDocument> lDocument(new AssetDocument(InAbsPath, lContents.Class));
            const OpaaxStringID       lName = NameOf(InAbsPath);

            if (InClasses.FindByName(lContents.Class) == nullptr)
            {
                // Kept as it is: opened read-only, so nothing is lost.
                OPAAX_LOG(LogAssetDocument, Warn, "'{}' holds a '{}', which no module registered — shown read-only",
                          InAbsPath.CStr(), lContents.Class.CStr());
                lDocument->m_RawProperties = Move(lContents.Properties);
            }
            else
            {
                lDocument->m_Asset = AssetFile::Instantiate(lContents, InClasses, lName, &OutError);
                if (!lDocument->m_Asset.IsValid())
                {
                    return nullptr;
                }
            }

            lDocument->MarkSaved();
            return lDocument;
        }

        /**
         * A new asset of InClass at InAbsPath, every field at its default. Not written yet: dirty until
         * saved.
         * @return Null when InClass is not a concrete Asset
         */
        static TUniquePtr<AssetDocument> Create(const OpaaxString& InAbsPath, const ObjectClass& InClass)
        {
            // A class read from files of its own comes with its file: there is nothing to write.
            if (!InClass.IsChildOf(Asset::StaticClass()) || InClass.IsAbstract() || InClass.IsReadFromSourceFiles())
            {
                return nullptr;
            }

            TUniquePtr<AssetDocument> lDocument(new AssetDocument(InAbsPath, InClass.GetName()));
            lDocument->m_Asset = Cast<Asset>(InClass.CreateInstance(NameOf(InAbsPath)));
            return lDocument;
        }

        // =============================================================================
        // Get
        // =============================================================================
    public:
        const OpaaxString& GetAbsPath() const noexcept { return m_AbsPath; }

        /** Just the file name, for the window title. */
        OpaaxString GetFileName() const { return PathString::FileName(m_AbsPath).ToString(); }

        OpaaxStringID GetClassName() const noexcept { return m_Class; }

        /** False for a class no module registered: then nothing can be edited or saved. */
        bool IsClassKnown() const noexcept { return m_Asset.IsValid(); }

        /** True for a file of its class's own format (a .png): shown, never edited or written. */
        bool IsFromSourceFile() const noexcept { return m_bFromSourceFile; }

        /** The editable copy; null when the class is unknown. */
        Asset* GetAsset() const noexcept { return m_Asset.Get(); }

        /** The current values (the raw ones when the class is unknown). */
        nlohmann::json GetProperties() const
        {
            return m_Asset.IsValid() ? m_Asset->GetClass().WriteProperties(*m_Asset) : m_RawProperties;
        }

        /** The exact text Save writes. */
        OpaaxString Serialize() const { return AssetFile::Serialize(m_Class, GetProperties()); }

        /** Whether the values differ from what was last written. Recomputed, never cached. */
        bool IsDirty() const { return !m_bFromSourceFile && Serialize() != m_Baseline; }

        // =============================================================================
        // Changes
        // =============================================================================
    public:
        /** Takes the current values as written. */
        void MarkSaved() { m_Baseline = Serialize(); }

        /**
         * Writes the file and takes the values as written.
         * @return False for an unknown class (never rewritten) or when the file cannot be written
         */
        bool Save()
        {
            if (m_bFromSourceFile)
            {
                OPAAX_LOG(LogAssetDocument, Warn, "'{}' is not saved: it is read from its own file, never rewritten",
                          m_AbsPath.CStr());
                return false;
            }

            if (!m_Asset.IsValid())
            {
                OPAAX_LOG(LogAssetDocument, Warn, "'{}' is not saved: its class '{}' is unknown",
                          m_AbsPath.CStr(), m_Class.CStr());
                return false;
            }

            if (!AssetFile::Save(m_AbsPath, *m_Asset))
            {
                return false;   // AssetFile logged why
            }

            MarkSaved();
            return true;
        }

        /**
         * Replaces the values (an undo, a redo, a script's edit), then runs PostLoad. All or nothing: a
         * value of the wrong type leaves every value as it was.
         * @param OutError The fields refused, when it fails (optional)
         * @return False for an unknown class, a non-object, or a wrong value type
         */
        bool SetProperties(const nlohmann::json& InProperties, OpaaxString* OutError = nullptr)
        {
            if (!m_Asset.IsValid())
            {
                if (OutError != nullptr) { *OutError = "the asset's class is unknown: it is read-only"; }
                return false;
            }

            if (m_bFromSourceFile)
            {
                if (OutError != nullptr) { *OutError = "the asset is read from its own file: it is read-only"; }
                return false;
            }

            const ObjectClass&     lClass  = m_Asset->GetClass();
            const nlohmann::json   lBefore = lClass.WriteProperties(*m_Asset);
            TDynArray<OpaaxString> lBadFields;

            if (!lClass.ReadProperties(*m_Asset, InProperties, &lBadFields))
            {
                if (OutError != nullptr) { *OutError = "the properties are not a JSON object"; }
                return false;
            }

            if (!lBadFields.empty())
            {
                lClass.ReadProperties(*m_Asset, lBefore);

                if (OutError != nullptr)
                {
                    *OutError = "wrong value type for";
                    for (const OpaaxString& lField : lBadFields) { *OutError += " " + lField; }
                }
                return false;
            }

            m_Asset->PostLoad();
            return true;
        }

        // =============================================================================
        // Members
        // =============================================================================
    private:
        AssetDocument(OpaaxString InAbsPath, const OpaaxStringID InClass)
            : m_AbsPath(Move(InAbsPath)), m_Class(InClass)
        {
        }

        /** A file of a class's own format (a .png): read by the class that reads InExtension. */
        static TUniquePtr<AssetDocument> OpenSource(const OpaaxString& InAbsPath, const OpaaxStringID InExtension,
                                                    const ObjectClassRegistry& InClasses, OpaaxString& OutError)
        {
            const ObjectClass* lClass = InClasses.FindBySourceExtension(InExtension);
            if (lClass == nullptr)
            {
                OutError = OpaaxString::Format("no asset class reads '{}' files", InExtension.CStr());
                return nullptr;
            }

            TStrongObjectPtr<Asset> lAsset = Cast<Asset>(lClass->CreateInstance(NameOf(InAbsPath)));
            if (!lAsset.IsValid() || !lAsset->LoadSource(InAbsPath.CStr()))
            {
                OutError = OpaaxString::Format("'{}' could not be read as a {} (see the log)", InAbsPath.CStr(),
                                               lClass->GetName().CStr());
                return nullptr;
            }

            // Opened on the main thread, so what the asset needs it for happens now.
            lAsset->InitializeOnMainThread();

            TUniquePtr<AssetDocument> lDocument(new AssetDocument(InAbsPath, lClass->GetName()));
            lDocument->m_Asset           = Move(lAsset);
            lDocument->m_bFromSourceFile = true;
            lDocument->MarkSaved();
            return lDocument;
        }

        static OpaaxStringID NameOf(const OpaaxString& InAbsPath)
        {
            return OpaaxStringID(PathString::Stem(InAbsPath).ToString());
        }

        OpaaxString             m_AbsPath;
        OpaaxStringID           m_Class;
        TStrongObjectPtr<Asset> m_Asset;

        /** The file's properties, when its class is unknown. */
        nlohmann::json          m_RawProperties = nlohmann::json::object();

        /** The text as of the last open or save: what IsDirty compares against. */
        OpaaxString             m_Baseline;

        /** Read from a file of its class's own format: never edited nor written. */
        bool                    m_bFromSourceFile = false;
    };
}
