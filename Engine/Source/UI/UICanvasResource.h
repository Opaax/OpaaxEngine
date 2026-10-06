#pragma once

#include <optional>

#include "Core/Log/Logger.h"
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
    // UICanvasResource — a .opaaxui as a resource. Holds the file text, not a widget tree:
    //   each BuildTree parses a new independent tree.
    // =============================================================================
    struct UICanvasResource final
    {
        /** The file contents. */
        OpaaxString Text;

        // ---- CResource contract --------------------------------------------------
        OPAAX_RESOURCE_FORMAT("UI Canvas", UICanvasFile::UI_EXTENSION)

        static constexpr EFailPolicy FailPolicy = EFailPolicy::Placeholder;

        static std::optional<UICanvasResource> Load(const char* InPath, LoadContext& /*InCtx*/)
        {
            // Only read the bytes: parsing needs the widget registry (done in BuildTree).
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

        /** An empty canvas: builds a root with nothing in it. */
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
         * Parses a new tree from the text.
         * @param OutReferenceHeight The canvas height the file was designed for
         * @return The root, or null if the text does not parse
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
