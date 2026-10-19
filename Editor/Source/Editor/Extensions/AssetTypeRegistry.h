#pragma once

#include "Core/OpaaxTypes.h"
#include "Core/String/Char.h"
#include "Core/String/OpaaxString.hpp"
#include "Object/Object.h"
#include "Editor/Assets/IAssetEditor.h"

namespace Opaax::Editor
{
    struct EditorContext;

    /** Builds an asset class's editor for one open asset. */
    using FAssetEditorFactory = TFunction<TUniquePtr<IAssetEditor>(EditorContext&)>;

    /** What SetEditor<T> accepts: an asset editor built from the context only. */
    template<typename T>
    concept CAssetEditor = CDerivedFrom<T, IAssetEditor> && CConstructibleFrom<T, EditorContext&>;

    // =============================================================================
    // AssetTypeDesc — how the editor shows one asset class: every field optional. A class registers
    //   nothing to be edited: its label and glyph come from its name, its editor is the property one.
    // =============================================================================
    struct AssetTypeDesc
    {
        const ObjectClass*  Class = nullptr;
        OpaaxString         Label;      // "Move Mode"
        OpaaxString         Glyph;      // "[MM]", drawn when there is no icon
        OpaaxString         Icon;       // editor-assets-relative image ("Icons/T_SpriteSheet_Icon.png")
        OpaaxString         Category;   // the New Asset submenu ("Movement")
        FAssetEditorFactory Editor;     // empty: the property editor
    };

    class AssetTypeRegistry;

    // =============================================================================
    // AssetTypeBuilder — the chained tail of AssetTypes().Register<T>(). Holds an index (the next
    //   Register may move the array).
    // =============================================================================
    class AssetTypeBuilder
    {
    public:
        AssetTypeBuilder(AssetTypeRegistry& InRegistry, const Uint64 InIndex) noexcept
            : m_Registry(InRegistry), m_Index(InIndex) {}

        AssetTypeBuilder& SetLabel(OpaaxString InLabel);
        AssetTypeBuilder& SetGlyph(OpaaxString InGlyph);
        AssetTypeBuilder& SetIcon(OpaaxString InIcon);
        AssetTypeBuilder& SetCategory(OpaaxString InCategory);

        /** The class's own editor (its subclasses use it too, unless they register theirs). */
        template<CAssetEditor TEditor>
        AssetTypeBuilder& SetEditor()
        {
            return SetEditorFactory([](EditorContext& InContext) -> TUniquePtr<IAssetEditor>
            {
                return MakeUnique<TEditor>(InContext);
            });
        }

        AssetTypeBuilder& SetEditorFactory(FAssetEditorFactory InFactory);

    private:
        AssetTypeRegistry& m_Registry;
        Uint64             m_Index;
    };

    // =============================================================================
    // AssetTypeRegistry — the storage behind EditorExtensionRegistrar::AssetTypes(). Answers "how does
    //   this asset class look, and what edits it?". A class not registered gets defaults from its
    //   name; a subclass inherits its parent's category, icon and editor.
    // =============================================================================
    class AssetTypeRegistry
    {
    public:
        /** Registers T's look (once: a second registration of a class returns the first's builder). */
        template<CObject T>
        AssetTypeBuilder Register()
        {
            return Register(T::StaticClass());
        }

        AssetTypeBuilder Register(const ObjectClass& InClass)
        {
            for (Uint64 lIndex = 0; lIndex < m_Entries.size(); ++lIndex)
            {
                if (m_Entries[lIndex].Class == &InClass) { return AssetTypeBuilder(*this, lIndex); }
            }

            m_Entries.emplace_back(AssetTypeDesc{ .Class = &InClass });
            return AssetTypeBuilder(*this, static_cast<Uint64>(m_Entries.size()) - 1);
        }

        // =============================================================================
        // Lookup
        // =============================================================================

        /** InClass's own entry, or null. */
        const AssetTypeDesc* FindExact(const ObjectClass& InClass) const noexcept
        {
            for (const AssetTypeDesc& lEntry : m_Entries)
            {
                if (lEntry.Class == &InClass) { return &lEntry; }
            }
            return nullptr;
        }

        /** "Move Mode": its own label, else its name split into words. */
        OpaaxString LabelOf(const ObjectClass& InClass) const
        {
            const AssetTypeDesc* lEntry = FindExact(InClass);
            return (lEntry != nullptr && !lEntry->Label.IsEmpty()) ? lEntry->Label : DefaultLabel(InClass);
        }

        /** "[MM]": its own glyph, else its name's capitals and digits. */
        OpaaxString GlyphOf(const ObjectClass& InClass) const
        {
            const AssetTypeDesc* lEntry = FindExact(InClass);
            return (lEntry != nullptr && !lEntry->Glyph.IsEmpty()) ? lEntry->Glyph : DefaultGlyph(InClass);
        }

        /** Its icon, or its nearest parent's; empty when none has one. */
        OpaaxString IconOf(const ObjectClass& InClass) const
        {
            return Inherited(InClass, &AssetTypeDesc::Icon);
        }

        /** Its New Asset submenu, or its nearest parent's; empty for the top level. */
        OpaaxString CategoryOf(const ObjectClass& InClass) const
        {
            return Inherited(InClass, &AssetTypeDesc::Category);
        }

        /** Its editor, or its nearest parent's; null for the property editor. */
        const FAssetEditorFactory* EditorOf(const ObjectClass& InClass) const
        {
            for (const ObjectClass* lClass = &InClass; lClass != nullptr; lClass = lClass->GetSuper())
            {
                const AssetTypeDesc* lEntry = FindExact(*lClass);
                if (lEntry != nullptr && lEntry->Editor) { return &lEntry->Editor; }
            }
            return nullptr;
        }

        const TDynArray<AssetTypeDesc>& Entries() const noexcept { return m_Entries; }

        // =============================================================================
        // Defaults
        // =============================================================================

        /** The class name split into words: "AudioClipDef" -> "Audio Clip Def", "UIWidget" -> "UI Widget". */
        static OpaaxString DefaultLabel(const ObjectClass& InClass)
        {
            const OpaaxStringView lName = InClass.GetName().GetView();
            OpaaxString           lLabel;

            for (Uint32 lIndex = 0; lIndex < lName.GetLength(); ++lIndex)
            {
                const char lChar = lName[lIndex];

                if (lIndex > 0 && Char::IsUpper(lChar))
                {
                    const char lPrevious = lName[lIndex - 1];
                    const bool bNextLower = lIndex + 1 < lName.GetLength() && Char::IsLower(lName[lIndex + 1]);

                    // A new word after a lowercase letter, or at the last capital of an acronym.
                    if (Char::IsLower(lPrevious) || (Char::IsUpper(lPrevious) && bNextLower))
                    {
                        lLabel += " ";
                    }
                }

                lLabel += lChar;
            }

            return lLabel;
        }

        /** The class name's capitals and digits in brackets, at most four: "MoveMode" -> "[MM]". */
        static OpaaxString DefaultGlyph(const ObjectClass& InClass)
        {
            const OpaaxStringView lName = InClass.GetName().GetView();
            OpaaxString           lGlyph("[");
            Uint32                lCount = 0;

            for (Uint32 lIndex = 0; lIndex < lName.GetLength() && lCount < 4; ++lIndex)
            {
                if (Char::IsUpper(lName[lIndex]) || Char::IsDigit(lName[lIndex]))
                {
                    lGlyph += lName[lIndex];
                    ++lCount;
                }
            }

            lGlyph += "]";
            return lGlyph;
        }

    private:
        friend class AssetTypeBuilder;

        OpaaxString Inherited(const ObjectClass& InClass, OpaaxString AssetTypeDesc::* InMember) const
        {
            for (const ObjectClass* lClass = &InClass; lClass != nullptr; lClass = lClass->GetSuper())
            {
                const AssetTypeDesc* lEntry = FindExact(*lClass);
                if (lEntry != nullptr && !(lEntry->*InMember).IsEmpty()) { return lEntry->*InMember; }
            }
            return OpaaxString();
        }

        TDynArray<AssetTypeDesc> m_Entries;
    };

    // =============================================================================
    // AssetTypeBuilder — bodies, now that the registry is complete.
    // =============================================================================
    inline AssetTypeBuilder& AssetTypeBuilder::SetLabel(OpaaxString InLabel)
    {
        m_Registry.m_Entries[m_Index].Label = Move(InLabel);
        return *this;
    }

    inline AssetTypeBuilder& AssetTypeBuilder::SetGlyph(OpaaxString InGlyph)
    {
        m_Registry.m_Entries[m_Index].Glyph = Move(InGlyph);
        return *this;
    }

    inline AssetTypeBuilder& AssetTypeBuilder::SetIcon(OpaaxString InIcon)
    {
        m_Registry.m_Entries[m_Index].Icon = Move(InIcon);
        return *this;
    }

    inline AssetTypeBuilder& AssetTypeBuilder::SetCategory(OpaaxString InCategory)
    {
        m_Registry.m_Entries[m_Index].Category = Move(InCategory);
        return *this;
    }

    inline AssetTypeBuilder& AssetTypeBuilder::SetEditorFactory(FAssetEditorFactory InFactory)
    {
        m_Registry.m_Entries[m_Index].Editor = Move(InFactory);
        return *this;
    }
}
