#include "OPpch.h"
#include "Opaax/Imgui/OpaaxImguiVulkan.h"

#include <imgui_impl_vulkan.h>

#include "Opaax/Renderer/Vulkan/OpaaxVulkanInclude.h"
#include "Opaax/Renderer/Vulkan/OpaaxVulkanMacro.h"

using namespace OPAAX::IMGUI;

void OpaaxImguiVulkan::Initialize(SDL_Window* SLDWindow, VkInstance Instance, VkPhysicalDevice PhysicalDevice, VkDevice Device, VkQueue GraphicsQueue, VkFormat Format)
{
	m_vkDevice = Device;
	 
	// 1: create descriptor pool for IMGUI
	//  the size of the pool is very oversize, but it's copied from imgui demo
	//  itself.
	VkDescriptorPoolSize lPoolSizes[] = {
	    { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
	};

	VkDescriptorPoolCreateInfo lPoolInfo = {};
	lPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	lPoolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	lPoolInfo.maxSets = 1000;
	lPoolInfo.poolSizeCount = static_cast<UInt32>(std::size(lPoolSizes));
	lPoolInfo.pPoolSizes = lPoolSizes;
	
	VK_CHECK(vkCreateDescriptorPool(Device, &lPoolInfo, nullptr, &m_vkPool));

	// 2: initialize imgui library

	// this initializes the core structures of imgui
	ImGui::CreateContext();

	// this initializes imgui for SDL
	ImGui_ImplSDL3_InitForVulkan(SLDWindow);

	// this initializes imgui for Vulkan
	ImGui_ImplVulkan_InitInfo lInitInfo = {};
	lInitInfo.Instance = Instance;
	lInitInfo.PhysicalDevice = PhysicalDevice;
	lInitInfo.Device = Device;
	lInitInfo.Queue = GraphicsQueue;
	lInitInfo.DescriptorPool = m_vkPool;
	lInitInfo.MinImageCount = 3;
	lInitInfo.ImageCount = 3;
	lInitInfo.UseDynamicRendering = true;

	//dynamic rendering parameters for imgui to use
	lInitInfo.PipelineRenderingCreateInfo = {.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
	lInitInfo.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
	lInitInfo.PipelineRenderingCreateInfo.pColorAttachmentFormats = &Format;
	lInitInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

	ImGui_ImplVulkan_Init(&lInitInfo);
	ImGui_ImplVulkan_CreateFontsTexture();

	// add to destroy the imgui created structures
	m_mainDeletionQueue.PushFunction([this]() {
		ImGui_ImplVulkan_Shutdown();
		vkDestroyDescriptorPool(m_vkDevice, m_vkPool, nullptr);
	});
}

void OpaaxImguiVulkan::Shutdown()
{
	m_mainDeletionQueue.Flush();
}
