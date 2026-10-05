#pragma once

#include "Core/EngineAPI.h"
#include "Core/Log/Logger.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxStringID.hpp"
#include "Engine/Subsystems/Resources/Types/DataAsset/DataAsset.h"

namespace Opaax
{
    inline constexpr LogCategory LogDataAssetTypeRegistry{"DataAssetTypeRegistry"};

    // =============================================================================
    // IDataAssetTypeEntry — one registered data asset type, without its C++ type: what the editor
    //   needs to create, read and draw any of them.
    // =============================================================================
    class IDataAssetTypeEntry
    {
    public:
        virtual ~IDataAssetTypeEntry() = default;

        /** The name written in files ("JumpTuning"). */
        virtual OpaaxStringID GetName() const noexcept = 0;

        virtual TypeId GetTypeId() const noexcept = 0;

        /** A default-constructed value. */
        virtual TUniquePtr<IDataAssetObject> Create() const = 0;

        /** A value read from a file's "Data". Null (and OutError set) if a value has the wrong type. */
        virtual TUniquePtr<IDataAssetObject> CreateFromJson(const nlohmann::json& InData, OpaaxString* OutError) const = 0;
    };

    template<CDataAsset T>
    class TDataAssetTypeEntry final : public IDataAssetTypeEntry
    {
    public:
        TDataAssetTypeEntry() : m_Name(DataAssetTypeName<T>()) {}

        OpaaxStringID GetName() const noexcept override   { return m_Name; }
        TypeId        GetTypeId() const noexcept override { return TypeIdOf<T>(); }

        TUniquePtr<IDataAssetObject> Create() const override { return MakeUnique<TDataAssetObject<T>>(); }

        TUniquePtr<IDataAssetObject> CreateFromJson(const nlohmann::json& InData, OpaaxString* OutError) const override
        {
            return ReadDataAsset<T>(InData, OutError);
        }

    private:
        OpaaxStringID m_Name;
    };

    // =============================================================================
    // DataAssetTypeRegistry — the struct types a .opaaxdata file can hold. Registering is the only
    //   step a game takes: the file format, the editor panel and "Create" all come from here.
    // =============================================================================
    class OPAAX_API DataAssetTypeRegistry
    {
    public:
        DataAssetTypeRegistry() = default;
        ~DataAssetTypeRegistry() = default;

        DataAssetTypeRegistry(const DataAssetTypeRegistry&)            = delete;
        DataAssetTypeRegistry& operator=(const DataAssetTypeRegistry&) = delete;

        // =============================================================================
        // Registration
        // =============================================================================
    public:
        /**
         * Registers T. Refused (and logged) once sealed, or if the name or type is already registered.
         */
        template<CDataAsset T>
        bool Register()
        {
            return AddEntry(MakeUnique<TDataAssetTypeEntry<T>>());
        }

        /** Called with the other registries before the first world. Safe to call twice. */
        void Seal() noexcept;

        // =============================================================================
        // Lookup
        // =============================================================================
    public:
        const IDataAssetTypeEntry* FindByName(OpaaxStringID InName) const noexcept;
        const IDataAssetTypeEntry* FindByTypeId(TypeId InTypeId) const noexcept;

        /** Every entry, in registration order. */
        template<typename TFunc>
        void ForEach(TFunc&& InFunc) const
        {
            for (const TUniquePtr<IDataAssetTypeEntry>& lEntry : m_Entries)
            {
                InFunc(static_cast<const IDataAssetTypeEntry&>(*lEntry));
            }
        }

        Uint64 Count()    const noexcept { return static_cast<Uint64>(m_Entries.size()); }
        bool   IsSealed() const noexcept { return m_bSealed; }

    private:
        bool AddEntry(TUniquePtr<IDataAssetTypeEntry> InEntry);

        TDynArray<TUniquePtr<IDataAssetTypeEntry>> m_Entries;
        bool                                      m_bSealed = false;
    };
}
