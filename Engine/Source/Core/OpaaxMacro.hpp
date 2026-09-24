#pragma once

#define CONCAT_HELPER(x, y) x##y

// The expanding form: CONCAT_HELPER pastes its arguments RAW, so a macro argument
// (__LINE__, __COUNTER__) needs this extra pass to become its value first.
#define OPAAX_CONCAT(x, y) CONCAT_HELPER(x, y)

#define STR(x) STR_HELPER(x)
#define STR_HELPER(x) #x

#define STR_CONCAT(x, y) STR(CONCAT_HELPER(x, y))