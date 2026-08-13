#pragma once
#include "Core/String/OpaaxStringID.hpp"

namespace Opaax::Editor
{

    struct EditorMenuCategory
    {
        explicit EditorMenuCategory(const OpaaxStringID Id)
            : m_ID(Id) {}

        EditorMenuCategory(const EditorMenuCategory& Other)
            : m_ID{Other.m_ID} {}

        EditorMenuCategory(EditorMenuCategory&& Other) noexcept = default;

        EditorMenuCategory& operator=(const EditorMenuCategory& Other)
        {
            if (this == &Other)
            {
                return *this;
            }
            
            m_ID = Other.m_ID;
            return *this;
        }

        EditorMenuCategory& operator=(EditorMenuCategory&& Other) noexcept
        {
            
        }
        
        // =============================================================================
        // Function
        // =============================================================================
        
        // =============================================================================
        // Getter
        
        OpaaxStringID GetID() const { return m_ID; }
        OpaaxString GetAsString() const { return m_ID.ToString(); }
        
        // End Getter 
        // =============================================================================

        // =============================================================================
        // Members
        // =============================================================================
    private:
        OpaaxStringID m_ID;
    };
    
    /**
     * @class EditorMenu
     * 
     * The Top bar menu that is not a panel in the Editor
     */
    class EditorMenu
    {
        // =============================================================================
        // Ctor - Dtor
        // =============================================================================
    public:
        EditorMenu();
        ~EditorMenu() = default;
        
        // =============================================================================
        // Functions
        // =============================================================================
    private:
        bool HasMenuCategories() const { return !MenuCategories.empty(); }
        bool IsCategoryAlreadyRegistered(const OpaaxStringID& InID) const;
        void RegisterCategoryInternal(TSharedPtr<EditorMenuCategory> InID);
        
    public:
        void RegisterMenuCategory(TSharedPtr<EditorMenuCategory> InID);
        
        bool DrawEditorMenu();
        
        // =============================================================================
        // Members
        // =============================================================================
    private:
        TDynArray<TSharedPtr<EditorMenuCategory>> MenuCategories = {};
    };
}
