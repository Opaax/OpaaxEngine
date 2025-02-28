#include "ConfigHelper.h"

#include <iostream>

bool ConfigHelper::IsCommentLine(const STDString& Line)
{
    // Do not read comments
    size_t lCommentPos = Line.find('#');
    
    return lCommentPos != std::string::npos;
}
