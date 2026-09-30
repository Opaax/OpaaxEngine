#include "Editor/EditorUICanvasDocument.h"

#include <algorithm>   // std::reverse

#include "Core/String/OpaaxPathString.h"
#include "UI/UICanvasFile.h"
#include "UI/UIWidget.h"
#include "UI/UIWidgetRegistry.h"

namespace Opaax::Editor
{
    namespace
    {
        /** Moves InSource's children onto InTarget. */
        void AdoptChildren(UIWidget& InTarget, UIWidget& InSource)
        {
            while (!InSource.GetChildren().empty())
            {
                UIWidget& lChild = *InSource.GetChildren().front();
                InTarget.AddChild(InSource.RemoveChild(lChild));
            }
        }
    }

    bool EditorUICanvasDocument::Open(const OpaaxString& InAbsPath, const UIWidgetRegistry& InRegistry)
    {
        UICanvasFile::UICanvasDoc lDoc;

        if (!UICanvasFile::Load(InAbsPath, InRegistry, lDoc))
        {
            return false;   // UICanvasFile logged why; the previous document stays open
        }

        // Clear first, then adopt: no widgets left from the previous canvas.
        while (!m_Canvas.Root().GetChildren().empty())
        {
            m_Canvas.Root().RemoveChild(*m_Canvas.Root().GetChildren().front());
        }

        m_Canvas.SetReferenceHeight(lDoc.ReferenceHeight);
        AdoptChildren(m_Canvas.Root(), *lDoc.Root);

        m_AbsPath = InAbsPath;
        m_Selected.clear();
        m_Baseline = Serialize();

        OPAAX_LOG(LogEditorUICanvasDocument, Info, "Opened '{}'", InAbsPath.CStr());
        return true;
    }

    void EditorUICanvasDocument::Close()
    {
        while (!m_Canvas.Root().GetChildren().empty())
        {
            m_Canvas.Root().RemoveChild(*m_Canvas.Root().GetChildren().front());
        }

        m_AbsPath.Clear();
        m_Baseline.Clear();
        m_Selected.clear();
    }

    void EditorUICanvasDocument::MarkSaved()
    {
        m_Baseline = Serialize();
    }

    bool EditorUICanvasDocument::RestoreFrom(const OpaaxString& InText, const UIWidgetRegistry& InRegistry)
    {
        UICanvasFile::UICanvasDoc lDoc;

        if (!UICanvasFile::Deserialize(InText, InRegistry, lDoc))
        {
            return false;
        }

        while (!m_Canvas.Root().GetChildren().empty())
        {
            m_Canvas.Root().RemoveChild(*m_Canvas.Root().GetChildren().front());
        }

        m_Canvas.SetReferenceHeight(lDoc.ReferenceHeight);
        AdoptChildren(m_Canvas.Root(), *lDoc.Root);

        // The tree was replaced; the path may still be valid (Resolve returns null if not).
        return true;
    }

    OpaaxString EditorUICanvasDocument::FileName() const
    {
        return m_AbsPath.IsEmpty() ? OpaaxString() : PathString::FileName(m_AbsPath).ToString();
    }

    OpaaxString EditorUICanvasDocument::Serialize() const
    {
        // The root overload: the canvas keeps its tree.
        return UICanvasFile::Serialize(m_Canvas.Root(), m_Canvas.GetReferenceHeight());
    }

    bool EditorUICanvasDocument::IsDirty() const
    {
        return IsOpen() && Serialize() != m_Baseline;
    }

    UIWidget* EditorUICanvasDocument::SelectedWidget()
    {
        return Resolve(m_Selected);
    }

    UIWidget* EditorUICanvasDocument::Resolve(const UIWidgetPath& InPath)
    {
        return const_cast<UIWidget*>(static_cast<const EditorUICanvasDocument&>(*this).Resolve(InPath));
    }

    const UIWidget* EditorUICanvasDocument::Resolve(const UIWidgetPath& InPath) const
    {
        const UIWidget* lNode = &m_Canvas.Root();

        for (const Uint32 lIndex : InPath)
        {
            if (lIndex >= lNode->GetChildren().size())
            {
                return nullptr;   // no longer there
            }

            lNode = lNode->GetChildren()[lIndex].get();
        }

        return lNode;
    }

    UIWidgetPath EditorUICanvasDocument::PathOf(const UIWidget& InWidget) const
    {
        UIWidgetPath lPath;

        for (const UIWidget* lNode = &InWidget; lNode->GetParent() != nullptr; lNode = lNode->GetParent())
        {
            const TDynArray<TUniquePtr<UIWidget>>& lSiblings = lNode->GetParent()->GetChildren();

            for (Uint32 lIndex = 0; lIndex < lSiblings.size(); ++lIndex)
            {
                if (lSiblings[lIndex].get() == lNode)
                {
                    lPath.emplace_back(lIndex);
                    break;
                }
            }
        }

        // Built leaf to root; a path reads root to leaf.
        std::reverse(lPath.begin(), lPath.end());
        return lPath;
    }

    namespace
    {
        /** Deepest visible descendant of InNode containing InPoint, last child first; null if none. */
        const UIWidget* DeepestAt(const UIWidget& InNode, const Vector2F& InPoint)
        {
            if (!InNode.bVisible)
            {
                return nullptr;
            }

            const TDynArray<TUniquePtr<UIWidget>>& lChildren = InNode.GetChildren();
            for (auto lIt = lChildren.rbegin(); lIt != lChildren.rend(); ++lIt)
            {
                if (const UIWidget* lHit = DeepestAt(**lIt, InPoint))
                {
                    return lHit;
                }
            }

            return InNode.GetBounds().Contains(InPoint) ? &InNode : nullptr;
        }
    }

    UIWidgetPath EditorUICanvasDocument::PickAt(const Vector2F& InCanvasPoint) const
    {
        const UIWidget* lHit = DeepestAt(m_Canvas.Root(), InCanvasPoint);

        // The root covers the whole canvas, so hitting the root means nothing was hit.
        return (lHit == nullptr || lHit == &m_Canvas.Root()) ? UIWidgetPath{} : PathOf(*lHit);
    }
}
