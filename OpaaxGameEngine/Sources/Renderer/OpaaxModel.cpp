#include "Renderer/OpaaxModel.h"

//#include "Renderer/Vulkan/OpaaxVulkanDevice.h"

//OpaaxModel::OpaaxModel(TUniquePtr<OpaaxVulkanDevice>& OpaaxDevice, const std::vector<Vertex> &Vertices)
//: m_opaaxDevice(OpaaxDevice)
//{
//  //CreateVertexBuffers(Vertices);
//}

OpaaxModel::~OpaaxModel() {
  //vkDestroyBuffer(m_opaaxDevice->GetLogicalDevice(), m_vertexBuffer, nullptr);
  //vkFreeMemory(m_opaaxDevice->GetLogicalDevice(), m_vertexBufferMemory, nullptr);
}

//void OpaaxModel::CreateVertexBuffers(const std::vector<Vertex> &Vertices)
//{
//  //m_vertexCount = static_cast<uint32_t>(Vertices.size());
//  //assert(m_vertexCount >= 3 && "Vertex count must be at least 3");
//  //VkDeviceSize lBufferSize = sizeof(Vertices[0]) * m_vertexCount;
//  //
//  //m_opaaxDevice->CreateBuffer(
//  //    lBufferSize,
//  //    VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
//  //    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
//  //    m_vertexBuffer,
//  //    m_vertexBufferMemory);
//  //
//  //void *data;
//  //vkMapMemory(m_opaaxDevice->GetLogicalDevice(), m_vertexBufferMemory, 0, lBufferSize, 0, &data);
//  //memcpy(data, Vertices.data(), static_cast<size_t>(lBufferSize));
//  //vkUnmapMemory(m_opaaxDevice->GetLogicalDevice(), m_vertexBufferMemory);
//}

void OpaaxModel::Draw(VkCommandBuffer CommandBuffer)
{
  //vkCmdDraw(CommandBuffer, m_vertexCount, 1, 0, 0);
}

void OpaaxModel::Bind(VkCommandBuffer CommandBuffer) {
  //VkBuffer lBuffers[] = {m_vertexBuffer};
  //VkDeviceSize lOffsets[] = {0};
  //vkCmdBindVertexBuffers(CommandBuffer, 0, 1, lBuffers, lOffsets);
}

//std::vector<VkVertexInputBindingDescription> OpaaxModel::Vertex::GetBindingDescriptions()
//{
//  std::vector<VkVertexInputBindingDescription> lBindingDescriptions(1);
//  lBindingDescriptions[0].binding = 0;
//  lBindingDescriptions[0].stride = sizeof(Vertex);
//  lBindingDescriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
//  return lBindingDescriptions;
//}

//std::vector<VkVertexInputAttributeDescription> OpaaxModel::Vertex::GetAttributeDescriptions()
//{
//  std::vector<VkVertexInputAttributeDescription> lAttributeDescriptions(1);
//  lAttributeDescriptions[0].binding = 0;
//  lAttributeDescriptions[0].location = 0;
//  lAttributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
//  lAttributeDescriptions[0].offset = 0;
//  return lAttributeDescriptions;
//}
