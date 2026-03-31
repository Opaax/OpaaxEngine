#include "Buffer.h"

namespace Opaax
{
    //------------------------------------------------------------------------------
    // BufferLayout
    
    void BufferLayout::CalculateOffsetsAndStride()
    {
        Uint32 lOffset = 0;
        m_Stride = 0;
        for (auto& lElement : m_Elements)
        {
            lElement.Offset = lOffset;
            lOffset += lElement.Size;
            m_Stride += lElement.Size;
        }
    }
}
