#include "UI/UICanvasFile.h"

#include <nlohmann/json.hpp>

#include "Core/IO/FileIO.h"
#include "Core/String/OpaaxStringJson.h"
#include "Object/ObjectClassRegistry.h"
#include "UI/UIWidget.h"
#include "UI/Widgets/UIPanel.h"

namespace Opaax
{
    namespace
    {
        constexpr const char* KEY_VERSION   = "Version";
        constexpr const char* KEY_REF_HEIGHT = "ReferenceHeight";
        constexpr const char* KEY_ROOT      = "Root";
        constexpr const char* KEY_TYPE      = "Type";
        constexpr const char* KEY_CHILDREN  = "Children";
        constexpr const char* KEY_MODIFIERS = "Modifiers";

        /** Version 1 to 2: an image's Color, Texture, Sheet, Frame, Border and BorderScale go into its Brush. */
        void UpgradeImageToBrush(nlohmann::json& InOutNode)
        {
            constexpr const char* FIELDS[][2] = { { "Color", "Tint" }, { "Texture", "Texture" }, { "Sheet", "Sheet" },
                                                  { "Frame", "Frame" }, { "Border", "Border" },
                                                  { "BorderScale", "BorderScale" } };

            // A key the node lacks stays out: the brush's default is version 1's.
            nlohmann::json lBrush = nlohmann::json::object();
            for (const auto& lField : FIELDS)
            {
                if (const auto lIt = InOutNode.find(lField[0]); lIt != InOutNode.end())
                {
                    lBrush[lField[1]] = Move(*lIt);
                    InOutNode.erase(lIt);
                }
            }
            InOutNode["Brush"] = Move(lBrush);
        }

        /**
         * Brings InOutNode and its subtree from InVersion to UI_FORMAT_VERSION, in place, before its
         * properties are read. Throws on a "Type" that is not text, as the reader does.
         */
        void UpgradeCanvasNode(nlohmann::json& InOutNode, const Uint32 InVersion)
        {
            if (InVersion >= UICanvasFile::UI_FORMAT_VERSION || !InOutNode.is_object())
            {
                return;
            }

            if (InVersion < 2 && InOutNode.value(KEY_TYPE, OpaaxString()) == OpaaxString("UIImage"))
            {
                UpgradeImageToBrush(InOutNode);
            }

            if (const auto lIt = InOutNode.find(KEY_CHILDREN); lIt != InOutNode.end() && lIt->is_array())
            {
                for (nlohmann::json& lChild : *lIt)
                {
                    UpgradeCanvasNode(lChild, InVersion);
                }
            }
        }

        /** One node: its class, its properties, its modifiers, then its children. */
        nlohmann::json WriteNode(const UIWidget& InWidget)
        {
            const ObjectClass& lClass = InWidget.GetClass();

            nlohmann::json lJson = lClass.WriteProperties(InWidget);
            lJson[KEY_TYPE] = lClass.GetName().CStr();

            // Optional: a node without modifiers is written as before, so older builds read it unchanged.
            const TDynArray<TStrongObjectPtr<UIModifier>>& lModifiers = InWidget.GetModifiers();
            if (!lModifiers.empty())
            {
                nlohmann::json lArray = nlohmann::json::array();
                for (const TStrongObjectPtr<UIModifier>& lModifier : lModifiers)
                {
                    const ObjectClass& lModifierClass = lModifier->GetClass();

                    nlohmann::json lModifierJson = lModifierClass.WriteProperties(*lModifier);
                    lModifierJson[KEY_TYPE] = lModifierClass.GetName().CStr();
                    lArray.emplace_back(Move(lModifierJson));
                }
                lJson[KEY_MODIFIERS] = Move(lArray);
            }

            const TDynArray<TStrongObjectPtr<UIWidget>>& lChildren = InWidget.GetChildren();
            if (!lChildren.empty())
            {
                nlohmann::json lArray = nlohmann::json::array();
                for (const TStrongObjectPtr<UIWidget>& lChild : lChildren)
                {
                    lArray.emplace_back(WriteNode(*lChild));
                }
                lJson[KEY_CHILDREN] = Move(lArray);
            }

            return lJson;
        }

        /**
         * Builds one node and its subtree. Null for an unknown type: the caller skips it (counted in
         * InOutSkipped). Null too for a value of the wrong type, named in OutBadValue: the read fails.
         */
        TStrongObjectPtr<UIWidget> ReadNode(const nlohmann::json& InJson, const ObjectClassRegistry& InClasses,
                                            Uint64& InOutSkipped, OpaaxString& OutBadValue)
        {
            if (!InJson.is_object()) { ++InOutSkipped; return nullptr; }

            const OpaaxString          lType   = InJson.value(KEY_TYPE, OpaaxString());
            TStrongObjectPtr<UIWidget> lWidget = UIWidget::Create(OpaaxStringID(lType), InClasses);

            if (!lWidget)
            {
                // Unknown type: skip this node, keep the file.
                OPAAX_LOG(LogUICanvasFile, Warn, "Unknown widget type '{}' — that node was skipped", lType.CStr());
                ++InOutSkipped;
                return nullptr;
            }

            // Strict like an asset file: a wrong value fails the read rather than leaving a default.
            TDynArray<OpaaxString> lBadFields;
            lWidget->GetClass().ReadProperties(*lWidget, InJson, &lBadFields);

            if (!lBadFields.empty())
            {
                OutBadValue = OpaaxString::Format("{}.{}", lType.CStr(), lBadFields.front().CStr());
                return nullptr;
            }

            lWidget->PostLoad();

            if (const auto lIt = InJson.find(KEY_MODIFIERS); lIt != InJson.end() && lIt->is_array())
            {
                for (const nlohmann::json& lModifierJson : *lIt)
                {
                    // Unknown type: skip this modifier (one warning), keep the widget and the rest.
                    const OpaaxString lModifierType = lModifierJson.is_object()
                                                          ? lModifierJson.value(KEY_TYPE, OpaaxString())
                                                          : OpaaxString();
                    TStrongObjectPtr<UIModifier> lModifier = UIModifier::Create(OpaaxStringID(lModifierType), InClasses);
                    if (!lModifier)
                    {
                        OPAAX_LOG(LogUICanvasFile, Warn, "Unknown modifier type '{}' on '{}' — that modifier was skipped",
                                  lModifierType.CStr(), lWidget->Name.CStr());
                        continue;
                    }

                    lModifier->GetClass().ReadProperties(*lModifier, lModifierJson, &lBadFields);
                    if (!lBadFields.empty())
                    {
                        OutBadValue = OpaaxString::Format("{}.{}.{}", lType.CStr(), lModifierType.CStr(),
                                                          lBadFields.front().CStr());
                        return nullptr;
                    }

                    lModifier->PostLoad();
                    lWidget->AddModifier(Move(lModifier));
                }
            }

            if (const auto lIt = InJson.find(KEY_CHILDREN); lIt != InJson.end() && lIt->is_array())
            {
                for (const nlohmann::json& lChildJson : *lIt)
                {
                    if (TStrongObjectPtr<UIWidget> lChild = ReadNode(lChildJson, InClasses, InOutSkipped, OutBadValue))
                    {
                        lWidget->AddChild(Move(lChild));
                    }
                    else if (!OutBadValue.IsEmpty())
                    {
                        return nullptr;
                    }
                }
            }

            return lWidget;
        }
    }

    OpaaxString UICanvasFile::Serialize(const UIWidget& InRoot, const float InReferenceHeight)
    {
        nlohmann::json lJson;
        lJson[KEY_VERSION]    = UI_FORMAT_VERSION;
        lJson[KEY_REF_HEIGHT] = InReferenceHeight;
        lJson[KEY_ROOT]       = WriteNode(InRoot);

        return ToJsonText(lJson, 4);
    }

    OpaaxString UICanvasFile::Serialize(const UICanvasDoc& InDoc)
    {
        if (!InDoc.Root)
        {
            nlohmann::json lJson;
            lJson[KEY_VERSION]    = UI_FORMAT_VERSION;
            lJson[KEY_REF_HEIGHT] = InDoc.ReferenceHeight;

            return ToJsonText(lJson, 4);
        }

        return Serialize(*InDoc.Root, InDoc.ReferenceHeight);
    }

    bool UICanvasFile::Deserialize(const OpaaxString& InText, const ObjectClassRegistry& InClasses, UICanvasDoc& OutDoc)
    {
        nlohmann::json lJson = nlohmann::json::parse(InText.CStr(), nullptr, false);

        if (lJson.is_discarded() || !lJson.is_object())
        {
            OPAAX_LOG(LogUICanvasFile, Error, "Not a json object — nothing was read");
            return false;
        }

        const Uint32 lVersion = lJson.value(KEY_VERSION, 0u);
        if (lVersion > UI_FORMAT_VERSION)
        {
            // Refuse a newer version rather than guessing.
            OPAAX_LOG(LogUICanvasFile, Error, "Format version {} is newer than this build reads ({})",
                      lVersion, UI_FORMAT_VERSION);
            return false;
        }

        UICanvasDoc lDoc;
        lDoc.ReferenceHeight = lJson.value(KEY_REF_HEIGHT, 1080.f);

        Uint64      lSkipped = 0;
        OpaaxString lBadValue;

        try
        {
            if (const auto lIt = lJson.find(KEY_ROOT); lIt != lJson.end())
            {
                UpgradeCanvasNode(*lIt, lVersion);
                lDoc.Root = ReadNode(*lIt, InClasses, lSkipped, lBadValue);
            }
        }
        catch (const nlohmann::json::exception& InError)
        {
            // A node's "Type" that is not text; OutDoc is untouched then.
            OPAAX_LOG(LogUICanvasFile, Error, "Unreadable value: {}", InError.what());
            return false;
        }

        if (!lBadValue.IsEmpty())
        {
            OPAAX_LOG(LogUICanvasFile, Error, "Unreadable value: {} has the wrong type — nothing was read",
                      lBadValue.CStr());
            return false;
        }

        if (!lDoc.Root)
        {
            OPAAX_LOG(LogUICanvasFile, Error, "No readable root — nothing was read");
            return false;
        }

        if (lSkipped > 0)
        {
            OPAAX_LOG(LogUICanvasFile, Warn, "{} node(s) were skipped — the rest of the tree loaded", lSkipped);
        }

        OutDoc = Move(lDoc);
        return true;
    }

    UICanvasFile::UICanvasDoc UICanvasFile::MakeEmpty(const float InReferenceHeight)
    {
        UICanvasDoc lDoc;
        lDoc.ReferenceHeight = InReferenceHeight;
        lDoc.Root            = NewObject<UIPanel>();
        lDoc.Root->Name      = "Root";

        // Stretched over its parent: mounted, the root is the whole screen.
        lDoc.Root->Rect.AnchorMin = { 0.f, 0.f };
        lDoc.Root->Rect.AnchorMax = { 1.f, 1.f };
        lDoc.Root->Rect.SizeDelta = { 0.f, 0.f };

        return lDoc;
    }

    bool UICanvasFile::Save(const OpaaxString& InAbsPath, const UICanvasDoc& InDoc)
    {
        if (!FileIO::WriteAllText(InAbsPath, Serialize(InDoc)))
        {
            OPAAX_LOG(LogUICanvasFile, Error, "Cannot write canvas '{}'", InAbsPath.CStr());
            return false;
        }

        OPAAX_LOG(LogUICanvasFile, Info, "Saved {} widget(s) to '{}'",
                  InDoc.Root ? CountWidgets(*InDoc.Root) : 0, InAbsPath.CStr());
        return true;
    }

    bool UICanvasFile::Load(const OpaaxString& InAbsPath, const ObjectClassRegistry& InClasses, UICanvasDoc& OutDoc)
    {
        const OpaaxString lText = FileIO::ReadAllText(InAbsPath);

        if (lText.IsEmpty())
        {
            OPAAX_LOG(LogUICanvasFile, Error, "Canvas '{}' is missing, empty or unreadable", InAbsPath.CStr());
            return false;
        }

        if (!Deserialize(lText, InClasses, OutDoc))
        {
            OPAAX_LOG(LogUICanvasFile, Error, "Canvas '{}' did not parse — nothing changed", InAbsPath.CStr());
            return false;
        }

        OPAAX_LOG(LogUICanvasFile, Trace, "Loaded {} widget(s) from '{}'",
                  OutDoc.Root ? CountWidgets(*OutDoc.Root) : 0, InAbsPath.CStr());
        return true;
    }

    Uint64 UICanvasFile::CountWidgets(const UIWidget& InRoot)
    {
        Uint64 lCount = 1;

        for (const TStrongObjectPtr<UIWidget>& lChild : InRoot.GetChildren())
        {
            lCount += CountWidgets(*lChild);
        }

        return lCount;
    }

    OpaaxString UICanvasFile::SerializeNode(const UIWidget& InWidget)
    {
        nlohmann::json lJson = WriteNode(InWidget);
        lJson[KEY_VERSION]   = UI_FORMAT_VERSION;
        return ToJsonText(lJson, 4);
    }

    TStrongObjectPtr<UIWidget> UICanvasFile::DeserializeNode(const OpaaxString& InText, const ObjectClassRegistry& InClasses)
    {
        nlohmann::json lJson = nlohmann::json::parse(InText.CStr(), nullptr, false);
        if (lJson.is_discarded() || !lJson.is_object())
        {
            return nullptr;   // not a node (e.g. unrelated clipboard text)
        }

        Uint64                     lSkipped = 0;
        OpaaxString                lBadValue;
        TStrongObjectPtr<UIWidget> lNode;
        try
        {
            // Copied by a build that wrote no version: version 1.
            const Uint32 lVersion = lJson.value(KEY_VERSION, 1u);
            if (lVersion > UI_FORMAT_VERSION)
            {
                OPAAX_LOG(LogUICanvasFile, Error, "Node format version {} is newer than this build reads ({})",
                          lVersion, UI_FORMAT_VERSION);
                return nullptr;
            }

            UpgradeCanvasNode(lJson, lVersion);
            lNode = ReadNode(lJson, InClasses, lSkipped, lBadValue);
        }
        catch (const nlohmann::json::exception& InError)
        {
            OPAAX_LOG(LogUICanvasFile, Error, "Unreadable node: {}", InError.what());
            return nullptr;
        }

        if (!lBadValue.IsEmpty())
        {
            OPAAX_LOG(LogUICanvasFile, Error, "Unreadable node: {} has the wrong type", lBadValue.CStr());
        }

        return lNode;
    }

    TStrongObjectPtr<UIWidget> UICanvasFile::CloneWidget(const UIWidget& InWidget, const ObjectClassRegistry& InClasses)
    {
        Uint64      lSkipped = 0;
        OpaaxString lBadValue;
        return ReadNode(WriteNode(InWidget), InClasses, lSkipped, lBadValue);
    }
}
