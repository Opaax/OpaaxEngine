#pragma once

#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"
#include "Object/ObjectPtr.h"
#include "Resources/Asset/Asset.h"
#include "UI/UICanvasFile.h"
#include "UI/UIWidget.h"

namespace Opaax
{
    class ObjectClassRegistry;

    // =============================================================================
    // UICanvasAsset — a .opaaxui as an asset, read from the file itself. Holds the file text, not a
    //   widget tree: each BuildTree parses a new tree, so every canvas that mounts it owns its widgets.
    //   A canvas that does not load mounts nothing.
    // =============================================================================
    class UICanvasAsset final : public Asset
    {
        OPAAX_OBJECT(UICanvasAsset, Asset)
        OPAAX_ASSET_SOURCE(UICanvasFile::UI_EXTENSION)

        // =========================================================================
        // CTORS - DTORS
        // =========================================================================
    public:
        UICanvasAsset();
        ~UICanvasAsset() override;

        // =========================================================================
        // Override
        // =========================================================================
        //~Begin Asset interface
    public:
        /** Reads the file's text; parsing waits for BuildTree, which is given the widget classes. */
        bool LoadSource(const char* InAbsPath) override;

        /** The text. */
        Uint64 GetResourceSize() const override;
        //~End Asset interface

        // =========================================================================
        // Use
        // =========================================================================
    public:
        /**
         * Parses a new tree from the text.
         * @param OutReferenceHeight The canvas height the file was designed for
         * @return The root, or null if the text does not parse (logged)
         */
        TStrongObjectPtr<UIWidget> BuildTree(const ObjectClassRegistry& InClasses, float& OutReferenceHeight) const;

        /** The file's text, as read. */
        const OpaaxString& GetText() const noexcept { return m_Text; }

        // =========================================================================
        // Members
        // =========================================================================
    private:
        OpaaxString m_Text;
    };
}
