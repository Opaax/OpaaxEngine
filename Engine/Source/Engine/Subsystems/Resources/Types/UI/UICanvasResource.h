#pragma once

#include <optional>

#include "Application/Services/ILogger.h"
#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"
#include "Core/IO/FileIO.h"
#include "Engine/Subsystems/Resources/ResourceFormat.h"
#include "UI/UICanvasFile.h"
#include "UI/UIWidget.h"

namespace Opaax
{
    class UIWidget;
    class UIWidgetRegistry;

    inline constexpr LogCategory LogUICanvasResource{"UICanvasResource"};

    // =============================================================================
    // UICanvasResource — a `.opaaxui` as a resource: the authored widget tree, as TEXT.
    //
    //   IT HOLDS THE TEXT, NOT A LIVE TREE, and that is the design rather than laziness: a widget
    //   is a non-copyable node that knows its parent and its canvas, so handing out "the resource's
    //   tree" would either share one mutable tree between every instance or need a deep clone per
    //   widget type. Re-reading the text is the clone (**UI13**) — every `BuildTree` yields an
    //   independent tree with no `Clone` override to keep in step as widget types are added.
    //
    //   A HUD is built once or twice per session, so the parse is not on any hot path; when one
    //   ever is, the cache goes HERE and no consumer changes.
    // =============================================================================
    struct UICanvasResource final
    {
        /** The file's bytes, verbatim. What BuildTree parses and what a reload replaces. */
        OpaaxString Text;

        // ---- CResource contract --------------------------------------------------
        OPAAX_RESOURCE_FORMAT("UI Canvas", UICanvasFile::UI_EXTENSION)

        static constexpr EFailPolicy FailPolicy = EFailPolicy::Placeholder;

        static std::optional<UICanvasResource> Load(const char* InPath, LoadContext& /*InCtx*/)
        {
            // Only the BYTES are taken here. Parsing needs the widget registry, which a resource
            // load has no route to — so it happens at BuildTree, where the caller has one (UI13).
            const OpaaxString lText = FileIO::ReadAllText(OpaaxString(InPath));

            if (lText.IsEmpty())
            {
                OPAAX_LOG(LogUICanvasResource, Error, "Canvas '{}' is missing, empty or unreadable", InPath);
                return std::nullopt;
            }

            UICanvasResource lResource;
            lResource.Text = lText;

            return lResource;
        }

        /** An empty canvas. It builds a tree with a root and nothing in it, never a null. */
        static UICanvasResource Placeholder()
        {
            UICanvasResource lResource;
            lResource.Text = R"({
    "ReferenceHeight": 1080.0,
    "Root": {
        "Type": "UIPanel"
    },
    "Version": 1
})";

            return lResource;
        }

        Uint64 ByteSize() const noexcept { return sizeof(UICanvasResource) + Text.GetLength(); }

        // ---- Use ------------------------------------------------------------------
        /**
         * Parse a FRESH tree out of the held text.
         *
         * @param OutReferenceHeight The canvas height the file was authored against.
         * @return The root, or null when the text does not parse. Independent of every other call.
         */
        TUniquePtr<UIWidget> BuildTree(const UIWidgetRegistry& InRegistry, float& OutReferenceHeight) const
        {
            UICanvasFile::UICanvasDoc lDoc;

            if (!UICanvasFile::Deserialize(Text, InRegistry, lDoc))
            {
                return nullptr;   // UICanvasFile logged why
            }

            OutReferenceHeight = lDoc.ReferenceHeight;

            return Move(lDoc.Root);
        }
    };
}
