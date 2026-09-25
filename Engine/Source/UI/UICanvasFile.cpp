#include "UI/UICanvasFile.h"

#include <nlohmann/json.hpp>

#include "Core/IO/FileIO.h"
#include "UI/UIWidget.h"
#include "UI/UIWidgetRegistry.h"

namespace Opaax
{
    namespace
    {
        constexpr const char* KEY_VERSION   = "Version";
        constexpr const char* KEY_REF_HEIGHT = "ReferenceHeight";
        constexpr const char* KEY_ROOT      = "Root";
        constexpr const char* KEY_TYPE      = "Type";
        constexpr const char* KEY_CHILDREN  = "Children";

        /** One node: its type tag, its own fields, then its children. */
        nlohmann::json WriteNode(const UIWidget& InWidget)
        {
            nlohmann::json lJson;
            lJson[KEY_TYPE] = InWidget.GetTypeName().CStr();

            InWidget.SaveFields(lJson);

            const TDynArray<TUniquePtr<UIWidget>>& lChildren = InWidget.GetChildren();
            if (!lChildren.empty())
            {
                nlohmann::json lArray = nlohmann::json::array();
                for (const TUniquePtr<UIWidget>& lChild : lChildren)
                {
                    lArray.emplace_back(WriteNode(*lChild));
                }
                lJson[KEY_CHILDREN] = Move(lArray);
            }

            return lJson;
        }

        /** Build one node and its subtree. Null when the type is unknown — the caller skips it. */
        TUniquePtr<UIWidget> ReadNode(const nlohmann::json& InJson, const UIWidgetRegistry& InRegistry,
                                      Uint64& InOutSkipped)
        {
            if (!InJson.is_object()) { ++InOutSkipped; return nullptr; }

            const OpaaxString   lType = OpaaxString(InJson.value(KEY_TYPE, std::string()).c_str());
            TUniquePtr<UIWidget> lWidget = InRegistry.Create(OpaaxStringID(lType));

            if (!lWidget)
            {
                // A type this build does not know: drop the NODE, keep the file (UI12).
                OPAAX_LOG(LogUICanvasFile, Warn, "Unknown widget type '{}' — that node was skipped", lType.CStr());
                ++InOutSkipped;
                return nullptr;
            }

            lWidget->LoadFields(InJson);

            if (const auto lIt = InJson.find(KEY_CHILDREN); lIt != InJson.end() && lIt->is_array())
            {
                for (const nlohmann::json& lChildJson : *lIt)
                {
                    if (TUniquePtr<UIWidget> lChild = ReadNode(lChildJson, InRegistry, InOutSkipped))
                    {
                        lWidget->AddChild(Move(lChild));
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

        return OpaaxString(lJson.dump(4).c_str());
    }

    OpaaxString UICanvasFile::Serialize(const UICanvasDoc& InDoc)
    {
        if (!InDoc.Root)
        {
            nlohmann::json lJson;
            lJson[KEY_VERSION]    = UI_FORMAT_VERSION;
            lJson[KEY_REF_HEIGHT] = InDoc.ReferenceHeight;

            return OpaaxString(lJson.dump(4).c_str());
        }

        return Serialize(*InDoc.Root, InDoc.ReferenceHeight);
    }

    bool UICanvasFile::Deserialize(const OpaaxString& InText, const UIWidgetRegistry& InRegistry, UICanvasDoc& OutDoc)
    {
        const nlohmann::json lJson = nlohmann::json::parse(InText.CStr(), nullptr, false);

        if (lJson.is_discarded() || !lJson.is_object())
        {
            OPAAX_LOG(LogUICanvasFile, Error, "Not a json object — nothing was read");
            return false;
        }

        const Uint32 lVersion = lJson.value(KEY_VERSION, 0u);
        if (lVersion > UI_FORMAT_VERSION)
        {
            // REFUSED, not guessed at: a newer shape may mean something different by the same key.
            OPAAX_LOG(LogUICanvasFile, Error, "Format version {} is newer than this build reads ({})",
                      lVersion, UI_FORMAT_VERSION);
            return false;
        }

        UICanvasDoc lDoc;
        lDoc.ReferenceHeight = lJson.value(KEY_REF_HEIGHT, 1080.f);

        Uint64 lSkipped = 0;

        try
        {
            if (const auto lIt = lJson.find(KEY_ROOT); lIt != lJson.end())
            {
                lDoc.Root = ReadNode(*lIt, InRegistry, lSkipped);
            }
        }
        catch (const nlohmann::json::exception& InError)
        {
            // A misspelled enumerator THROWS out of the enum bridge rather than reading as the first
            // value — which is what this catch is for, and why OutDoc is untouched on the way out.
            OPAAX_LOG(LogUICanvasFile, Error, "Unreadable value: {}", InError.what());
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

    bool UICanvasFile::Load(const OpaaxString& InAbsPath, const UIWidgetRegistry& InRegistry, UICanvasDoc& OutDoc)
    {
        const OpaaxString lText = FileIO::ReadAllText(InAbsPath);

        if (lText.IsEmpty())
        {
            OPAAX_LOG(LogUICanvasFile, Error, "Canvas '{}' is missing, empty or unreadable", InAbsPath.CStr());
            return false;
        }

        if (!Deserialize(lText, InRegistry, OutDoc))
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

        for (const TUniquePtr<UIWidget>& lChild : InRoot.GetChildren())
        {
            lCount += CountWidgets(*lChild);
        }

        return lCount;
    }

    OpaaxString UICanvasFile::SerializeNode(const UIWidget& InWidget)
    {
        return OpaaxString(WriteNode(InWidget).dump(4).c_str());
    }

    TUniquePtr<UIWidget> UICanvasFile::DeserializeNode(const OpaaxString& InText, const UIWidgetRegistry& InRegistry)
    {
        const nlohmann::json lJson = nlohmann::json::parse(InText.CStr(), nullptr, false);
        if (lJson.is_discarded() || !lJson.is_object())
        {
            return nullptr;   // not a node — a paste of unrelated clipboard text is a no-op
        }

        Uint64 lSkipped = 0;
        try
        {
            return ReadNode(lJson, InRegistry, lSkipped);
        }
        catch (const nlohmann::json::exception& InError)
        {
            OPAAX_LOG(LogUICanvasFile, Error, "Unreadable node: {}", InError.what());
            return nullptr;
        }
    }

    TUniquePtr<UIWidget> UICanvasFile::CloneWidget(const UIWidget& InWidget, const UIWidgetRegistry& InRegistry)
    {
        Uint64 lSkipped = 0;
        return ReadNode(WriteNode(InWidget), InRegistry, lSkipped);
    }
}
