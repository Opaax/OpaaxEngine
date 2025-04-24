#pragma once
#include "Opaax/OpaaxStdTypes.h"

namespace OPAAX
{
	/**
	 * @struct OpaaxWindowSpecs
	 * @brief A structure that defines the specifications for an OpaaxWindow.
	 *
	 * This structure is used to configure the basic properties of an Opaax window,
	 * including its width, height, and title.
	 *
	 * @details By default, the window is initialized with a width of 1280, a height
	 * of 720, and the title "Opaax Window". Custom specifications can be provided
	 * using the parameterized constructor.
	 */
	struct OpaaxWindowSpecs
	{
		Int32 Width{1280};
		Int32 Height{720};
		const char* Title{"Opaax Window"};
	
		OpaaxWindowSpecs() = default;
		explicit OpaaxWindowSpecs(Int32 InWidth, Int32 InHeight, const char* InTitle)
			: Width(InWidth), Height(InHeight), Title(InTitle) {}
	};
}