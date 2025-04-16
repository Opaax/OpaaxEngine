#pragma once

#include "Opaax/Log/OPLogger.h"

/**
 * Internal use.
 * 
 * @param Category The Log category
 * @param MESSAGE The message that will be logged.
 */
#define OPAAX_LOGGER_INTERNAL(Category, MESSAGE) \
 OPLogger::Instance().Log(Category, MESSAGE);

/**
 * Use boost format to permit log with params, on Log category
 *
 * E.G.
 * int MyInt = 12;
 * OPAAX_LOG("Luke %1% and Han %2%.", % LogInt % "Solo")
 *
 * You can use it without args as well
 * OPAAX_LOG("Luke")
 * @param FORMAT Boost format --- "Luke %1% and Han %2%.", % LogInt % "Solo"
 * @param ... Args to apply
 */
#define OPAAX_LOG(FORMAT, ...) \
 OPAAX_LOGGER_INTERNAL(ELogCategory::ELC_Info, (BSTFormat(##FORMAT) __VA_ARGS__).str())

/**
 * Use boost format to permit log with params, on Warning category
 *
 * E.G.
 * int MyInt = 12;
 * OPAAX_WARNING("Luke %1% and Han %2%.", % LogInt % "Solo")
 *
 * You can use it without args as well
 * OPAAX_WARNING("Luke")
 * @param FORMAT Boost format --- "Luke %1% and Han %2%.", % LogInt % "Solo"
 * @param ... Args to apply
 */
#define OPAAX_WARNING(FORMAT, ...) \
 OPAAX_LOGGER_INTERNAL(ELogCategory::ELC_Warning, (BSTFormat(##FORMAT) __VA_ARGS__).str());

/**
 * Use boost format to permit log with params, on error category
 *
 * E.G.
 * int MyInt = 12;
 * OPAAX_ERROR("Luke %1% and Han %2%.", % LogInt % "Solo")
 *
 * You can use it without args as well
 * OPAAX_ERROR("Luke")
 * @param FORMAT Boost format --- "Luke %1% and Han %2%.", % LogInt % "Solo"
 * @param ... Args to apply
 */
#define OPAAX_ERROR(FORMAT, ...) \
 OPAAX_LOGGER_INTERNAL(ELogCategory::ELC_Error, (BSTFormat(##FORMAT) __VA_ARGS__).str());

/**
 * Use boost format to permit log with params, on debug category
 *
 * E.G.
 * int MyInt = 12;
 * OPAAX_DEBUG("Luke %1% and Han %2%.", % LogInt % "Solo")
 *
 * You can use it without args as well
 * OPAAX_DEBUG("Luke")
 * @param FORMAT Boost format --- "Luke %1% and Han %2%.", % LogInt % "Solo"
 * @param ... Args to apply
 */
#define OPAAX_DEBUG(FORMAT, ...) \
 OPAAX_LOGGER_INTERNAL(ELogCategory::ELC_Debug, (BSTFormat(##FORMAT) __VA_ARGS__).str());

/**
 * Use boost format to permit log with params, on verbose category
 *
 * E.G.
 * int MyInt = 12;
 * OPAAX_VERBOSE("Luke %1% and Han %2%.", % LogInt % "Solo")
 *
 * You can use it without args as well
 * OPAAX_VERBOSE("Luke")
 * @param FORMAT Boost format --- "Luke %1% and Han %2%.", % LogInt % "Solo"
 * @param ... Args to apply
 */
#define OPAAX_VERBOSE(FORMAT, ...) \
 OPAAX_LOGGER_INTERNAL(ELogCategory::ELC_Verbose, (BSTFormat(##FORMAT) __VA_ARGS__).str());