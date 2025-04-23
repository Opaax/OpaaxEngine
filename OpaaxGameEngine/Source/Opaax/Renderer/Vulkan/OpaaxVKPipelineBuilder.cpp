#include "OPpch.h"
#include "Opaax/Renderer/Vulkan/OpaaxVKPipelineBuilder.h"

#include "Opaax/OpaaxStdTypes.h"
#include "Opaax/Log/OPLogMacro.h"

bool OPAAX::RENDERER::VULKAN_HELPER::LoadShaderModule(const char* FilePath, VkDevice Device, VkShaderModule* OutShaderModule)
{
    // open the file. With cursor at the end
    std::ifstream lFile(FilePath, std::ios::ate | std::ios::binary);

    if (!lFile.is_open())
    {
        OPAAX_ERROR("[LoadShaderModule] Failed to open file. Path: %1%", %FilePath)
        return false;
    }

    // find what the size of the file is by looking up the location of the cursor
    // because the cursor is at the end, it gives the size directly in bytes
    size_t lFileSize = (size_t)lFile.tellg();

    // spirv expects the buffer to be on uint32, so make sure to reserve a int
    // vector big enough for the entire file
    std::vector<UInt32> lBuffer(lFileSize / sizeof(UInt32));

    // put file cursor at beginning
    lFile.seekg(0);

    // load the entire file into the buffer
    lFile.read(reinterpret_cast<char*>(lBuffer.data()), lFileSize);

    // now that the file is loaded into the buffer, we can close it
    lFile.close();

    // create a new shader module, using the buffer we loaded
    VkShaderModuleCreateInfo lCreateInfo = {};
    lCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    lCreateInfo.pNext = nullptr;

    // codeSize has to be in bytes, so multply the ints in the buffer by size of
    // int to know the real size of the buffer
    lCreateInfo.codeSize = lBuffer.size() * sizeof(uint32_t);
    lCreateInfo.pCode = lBuffer.data();

    // check that the creation goes well.
    VkShaderModule ShaderModule;
    if (vkCreateShaderModule(Device, &lCreateInfo, nullptr, &ShaderModule) != VK_SUCCESS)
    {
        OPAAX_ERROR("[LoadShaderModule] Failed to create shader module! Path: %1%", %FilePath)
        return false;
    }
    
    *OutShaderModule = ShaderModule;
    
    return true;
}
