#pragma once

#define CONCAT_HELPER(x, y) x##y

// Expands its arguments first (for __LINE__, __COUNTER__), then concatenates.
#define OPAAX_CONCAT(x, y) CONCAT_HELPER(x, y)

#define STR(x) STR_HELPER(x)
#define STR_HELPER(x) #x

#define STR_CONCAT(x, y) STR(CONCAT_HELPER(x, y))