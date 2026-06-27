#pragma once

#include <cstdint>

#include "Core/EngineAPI.h"

namespace Opaax
{
	class OpaaxString;

	using ConfigTypeID = uintptr_t;

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
		 * File name within the project Configs/ dir (e.g. "engine.config.json").
		 * IConfigSystem resolves the absolute path as <ProjectRoot>/Configs/<FileName>.
		 */
		virtual const char* FileName() const = 0;

		/** Stable per-type id used as the key in IConfigSystem's registry. */
		virtual ConfigTypeID GetConfigTypeID() const noexcept = 0;

		/**
		 * Load the config from disk. If the file does not exist, a default file
		 * is generated at InAbsPath and the in-memory defaults are kept.
		 * @return true on success or after a successful default-generation;
		 *         false if a file existed but failed to parse.
		 */
		virtual bool Load(const OpaaxString& InAbsPath) = 0;

		/**
		 * Persist the current values back to InAbsPath.
		 */
		virtual bool Save(const OpaaxString& InAbsPath) = 0;

		/**
		 * Persist back to the path the config was last loaded from (set by Load).
		 * No-op + false if Load was never called. Used by the editor Config panel.
		 */
		virtual bool Save() = 0;

	};
}

// Stamp on each concrete config. StaticTypeID() is DEFINED out-of-line in the .cpp so
// there is one tag across the DLL/exe line.
#define OPAAX_CONFIG_TYPE(ClassName)                                        \
	static ::Opaax::ConfigTypeID StaticTypeID() noexcept;                   \
	::Opaax::ConfigTypeID GetConfigTypeID() const noexcept override         \
	{ return StaticTypeID(); }
