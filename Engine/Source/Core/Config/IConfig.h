#pragma once

#include <cstdint>

#include "Core/EngineAPI.h"
#include "Core/Events/Delegate.h"
#include "Core/String/OpaaxStringID.hpp"

namespace Opaax
{
	using ConfigTypeID = uintptr_t;

	// No payload: a subscriber already holds &GetData() and re-reads the fields it uses.
	DECLARE_MULTICAST_DELEGATE(FOnConfigChanged)

	class OPAAX_API IConfig
	{
		// =============================================================================
		// Ctor - dtor
		// =============================================================================
	public:
		IConfig()			= default;
		virtual ~IConfig()	= default;

		// =============================================================================
		// Copy - Move : Delete
		// =============================================================================
	private:
		IConfig(const IConfig&)				= delete;
		IConfig& operator=(const IConfig&)	= delete;

		IConfig(IConfig&&)				= delete;
		IConfig& operator=(IConfig&&)	= delete;

		// =============================================================================
		// Functions
		// =============================================================================
	protected:
		virtual bool GenerateDefaultConfig(const OpaaxString& InAbsPath) = 0;

	public:
		/**
		 * File name inside the project Configs/ dir (e.g. "Engine.config").
		 */
		virtual const char* FileName() const = 0;

		/** Stable per-type id used as the key in IConfigSystem's registry. */
		virtual ConfigTypeID GetConfigTypeID() const noexcept = 0;

		/**
		 * Display name: the file name without extension ("Engine.config" -> "Engine").
		 */
		OpaaxStringID GetName() const;

		/**
		 * The current values as text, same as what Save writes.
		 */
		virtual OpaaxString ToText() const = 0;

		/**
		 * Loads the config from disk. If the file is missing, writes a default one.
		 * @return False if the file exists but could not be parsed
		 */
		virtual bool Load(const OpaaxString& InAbsPath) = 0;

		/**
		 * Saves the current values to InAbsPath.
		 */
		virtual bool Save(const OpaaxString& InAbsPath) = 0;

		/**
		 * Saves to the path used by the last Load.
		 * @return False if Load was never called
		 */
		virtual bool Save() = 0;

		/**
		 * Fired after the values changed. Subscribe with AddMember(this, ...) and call
		 * RemoveAll(this) before the subscriber is destroyed. Main thread only.
		 */
		FOnConfigChanged& OnChanged() noexcept { return m_OnChanged; }

		/** Called by whoever changed the values (Config panel, Load). */
		void NotifyChanged() { m_OnChanged.Broadcast(); }

		// =============================================================================
		// Members
		// =============================================================================
	private:
		FOnConfigChanged m_OnChanged;
	};
}

// Add to each concrete config. Define StaticTypeID() in the .cpp so the ID is
// shared across the DLL/exe boundary.
#define OPAAX_CONFIG_TYPE(ClassName)                                        \
	static ::Opaax::ConfigTypeID StaticTypeID() noexcept;                   \
	::Opaax::ConfigTypeID GetConfigTypeID() const noexcept override         \
	{ return StaticTypeID(); }
