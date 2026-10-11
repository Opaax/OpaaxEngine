#pragma once

#include <type_traits>

#include <nlohmann/json.hpp>

#include "Core/Color/LinearColorJson.h"
#include "Core/GUID/GuidJson.h"
#include "Core/Maths/MathsJson.hpp"
#include "Core/OpaaxTypes.h"
#include "Core/Reflection/OpaaxEnumJson.h"
#include "Core/Reflection/OpaaxProperty.h"
#include "Core/String/OpaaxString.hpp"
#include "Core/String/OpaaxStringIDJson.h"
#include "Core/String/OpaaxStringJson.h"
#include "Core/Tag/OpaaxTagJson.h"
#include "Resources/ResourcePathJson.h"

namespace Opaax
{
    // =============================================================================
    // PropertyJson — a reflected type's JSON, read and written from its property list
    //   (OPAAX_PROPERTIES), so its fields are listed once. Only listed fields are saved.
    //   A nested reflected type is an object written the same way.
    // =============================================================================

    /**
     * Writes every listed field of InValue, keyed by field name.
     */
    template<CReflected T>
    nlohmann::json PropertiesToJson(const T& InValue);

    /**
     * Reads InJson into InOutValue's listed fields.
     * A missing key keeps the field's current value. A value of the wrong type keeps it too, and the
     * field's name ("Inner.Amount" for a nested one) is added to OutBadFields.
     * @return False if InJson is not an object (nothing is read)
     */
    template<CReflected T>
    bool PropertiesFromJson(const nlohmann::json& InJson, T& InOutValue, TDynArray<OpaaxString>* OutBadFields = nullptr);

    namespace PropertyJsonDetail
    {
        template<typename T>
        concept CJsonValue = requires(nlohmann::json& InJson, const T& InConst, T& InMutable)
        {
            InJson = InConst;
            InJson.get_to(InMutable);
        };

        inline OpaaxString JoinPath(const OpaaxString& InPrefix, const char* InName)
        {
            if (InPrefix.IsEmpty())
            {
                return OpaaxString(InName);
            }

            OpaaxString lPath = InPrefix;
            lPath += ".";
            lPath += InName;
            return lPath;
        }

        template<typename TValue>
        nlohmann::json ValueToJson(const TValue& InValue)
        {
            if constexpr (CReflected<TValue>)
            {
                return PropertiesToJson(InValue);
            }
            else if constexpr (CEnumWithValues<TValue>)
            {
                // Called by name: a game enum lives outside Opaax, where lookup would not find it.
                nlohmann::json lJson;
                ::Opaax::to_json(lJson, InValue);
                return lJson;
            }
            else
            {
                static_assert(CJsonValue<TValue>,
                              "A listed field has no JSON conversion: give its type to_json/from_json, "
                              "or OPAAX_PROPERTIES.");
                return nlohmann::json(InValue);
            }
        }

        template<CReflected T>
        bool ReadObject(const nlohmann::json& InJson, T& InOutValue, const OpaaxString& InPrefix,
                        TDynArray<OpaaxString>* OutBadFields);

        template<typename TValue>
        void ReadValue(const nlohmann::json& InJson, TValue& InOutValue, const OpaaxString& InPath,
                       TDynArray<OpaaxString>* OutBadFields)
        {
            if constexpr (CReflected<TValue>)
            {
                if (!ReadObject(InJson, InOutValue, InPath, OutBadFields) && OutBadFields != nullptr)
                {
                    OutBadFields->push_back(InPath);
                }
            }
            else
            {
                // Read into a copy: a value that fails halfway must not leave the field half-written.
                TValue lValue = InOutValue;
                try
                {
                    if constexpr (CEnumWithValues<TValue>) { ::Opaax::from_json(InJson, lValue); }
                    else                                   { InJson.get_to(lValue); }

                    InOutValue = Move(lValue);
                }
                catch (const nlohmann::json::exception&)
                {
                    if (OutBadFields != nullptr) { OutBadFields->push_back(InPath); }
                }
            }
        }

        template<CReflected T>
        bool ReadObject(const nlohmann::json& InJson, T& InOutValue, const OpaaxString& InPrefix,
                        TDynArray<OpaaxString>* OutBadFields)
        {
            if (!InJson.is_object())
            {
                return false;
            }

            std::apply([&](const auto&... lProperties)
                       {
                           ([&](const auto& InProperty)
                            {
                                const auto lIt = InJson.find(InProperty.Name);
                                if (lIt != InJson.end())
                                {
                                    ReadValue(*lIt, InOutValue.*(InProperty.Member),
                                              JoinPath(InPrefix, InProperty.Name), OutBadFields);
                                }
                            }(lProperties), ...);
                       },
                       T::GetProperties());
            return true;
        }
    }

    template<CReflected T>
    nlohmann::json PropertiesToJson(const T& InValue)
    {
        nlohmann::json lJson = nlohmann::json::object();

        std::apply([&](const auto&... lProperties)
                   {
                       ((lJson[lProperties.Name] = PropertyJsonDetail::ValueToJson(InValue.*(lProperties.Member))), ...);
                   },
                   T::GetProperties());
        return lJson;
    }

    template<CReflected T>
    bool PropertiesFromJson(const nlohmann::json& InJson, T& InOutValue, TDynArray<OpaaxString>* OutBadFields)
    {
        return PropertyJsonDetail::ReadObject(InJson, InOutValue, OpaaxString(), OutBadFields);
    }
}
