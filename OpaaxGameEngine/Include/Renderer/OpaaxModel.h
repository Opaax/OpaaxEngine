#pragma once

// libs
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <vulkan/vulkan_core.h>

#include "OpaaxTypes.h"

class OpaaxVulkanDevice;

class OpaaxModel
{
public:
    //struct Vertex
    //{
    //    glm::vec2 Position;
    //
    //    static std::vector<VkVertexInputBindingDescription> GetBindingDescriptions();
    //    static std::vector<VkVertexInputAttributeDescription> GetAttributeDescriptions();
    //};

    //OpaaxModel(TUniquePtr<OpaaxVulkanDevice>& OpaaxDevice, const std::vector<Vertex> &Vertices);
    ~OpaaxModel();

    OpaaxModel(const OpaaxModel&) = delete;
    OpaaxModel &operator=(const OpaaxModel&) = delete;

    void Bind(VkCommandBuffer CommandBuffer);
    void Draw(VkCommandBuffer CommandBuffer);

private:
    //void CreateVertexBuffers(const std::vector<Vertex> &Vertices);

    TUniquePtr<OpaaxVulkanDevice>& m_opaaxDevice;
    VkBuffer m_vertexBuffer;
    VkDeviceMemory m_vertexBufferMemory;
    UInt32 m_vertexCount;
};
