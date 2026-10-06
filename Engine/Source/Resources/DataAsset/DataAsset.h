#pragma once

#include <concepts>

#include <nlohmann/json.hpp>

#include "Core/OpaaxTypes.h"
#include "Core/Reflection/OpaaxProperty.h"
#include "Engine/Reflection/PropertyVisitor.h"
#include "Core/Reflection/TypeInfo.h"

namespace Opaax
{
    // =============================================================================
    // CDataAsset — a struct that can be a data asset: it lists its fields (OPAAX_PROPERTIES) and
    //   reads/writes JSON (NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT, so a missing key keeps its
    //   default). Its saved type name is its C++ name, without namespace.
    // =============================================================================
    template<typename T>
    concept CDataAsset = CReflected<T> && std::default_initializable<T>
        && requires(const T& InConst, T& InMutable, nlohmann::json& InJson)
        {
            InJson = InConst;
            InJson.get_to(InMutable);
        };

    /** The name written in a .opaaxdata file for T. */
    template<CDataAsset T>
    OpaaxStringID DataAssetTypeName()
    {
        return DeriveTypeLeafName<T>();
    }

    // =============================================================================
    // IDataAssetObject — one data asset's value, held without its C++ type (by the editor, which
    //   draws and saves it, and by a resource's typed view).
    // =============================================================================
    class IDataAssetObject
    {
    public:
        virtual ~IDataAssetObject() = default;

        virtual TypeId         GetTypeId() const noexcept = 0;
        virtual void           Visit(IPropertyVisitor& InVisitor) = 0;
        virtual nlohmann::json ToJson() const = 0;
    };

    template<CDataAsset T>
    class TDataAssetObject final : public IDataAssetObject
    {
    public:
        T Value;

        TypeId         GetTypeId() const noexcept override         { return TypeIdOf<T>(); }
        void           Visit(IPropertyVisitor& InVisitor) override { VisitProperties(Value, InVisitor); }
        nlohmann::json ToJson() const override                     { return nlohmann::json(Value); }
    };

    /**
     * Reads a T from InData. Missing keys keep their defaults; a value of the wrong JSON type fails.
     * @param OutError Why it failed, when it does (optional)
     * @return Null on failure
     */
    template<CDataAsset T>
    TUniquePtr<TDataAssetObject<T>> ReadDataAsset(const nlohmann::json& InData, OpaaxString* OutError = nullptr)
    {
        if (!InData.is_object())
        {
            if (OutError != nullptr) { *OutError = OpaaxString("\"Data\" is not an object"); }
            return nullptr;
        }

        TUniquePtr<TDataAssetObject<T>> lObject = MakeUnique<TDataAssetObject<T>>();

        try
        {
            InData.get_to(lObject->Value);
        }
        catch (const nlohmann::json::exception& InError)
        {
            if (OutError != nullptr) { *OutError = OpaaxString(InError.what()); }
            return nullptr;
        }

        return lObject;
    }
}
