#pragma once
#include "OpaaxVKAllocatedImage.h"
#include "OpaaxVKComputeEffect.h"
#include "OpaaxVKDescriptorAllocator.h"
#include "OpaaxVKGlobal.h"
#include "OpaaxVKTypes.h"
#include "OpaaxVulkanInclude.h"
#include "Opaax/Renderer/IOpaaxRendererContext.h"
#include "OpaaxVKFrameData.h"
#include "Opaax/OpaaxDeletionQueue.h"

namespace OPAAX
{
    namespace RENDERER
    {
        namespace VULKAN
        {
            /**
             * Represents a Vulkan-based renderer for the Opaax rendering engine.
             * This class is responsible for setting up, managing, and utilizing the Vulkan API features
             * to render graphical content efficiently.
             *
             * Functionality provided by the class includes:
             *   - Initialization and configuration of Vulkan instances, devices, surfaces, and swapchains.
             *   - Command buffer and synchronization mechanism management.
             *   - Allocation of Vulkan memory using Vulkan Memory Allocator (VMA).
             *   - Descriptor and pipeline generation for rendering and compute tasks.
             *   - Immediate submission of commands to the GPU.
             *   - Integration with ImGui for debug UI rendering.
             *   - Management of Vulkan swapchain for window resizing and presentation.
             *   - Handling of per-frame rendering data for multi-frame buffering.
             *
             * This class implements the OPAAX::IOpaaxRendererContext interface to provide the required rendering functionality.
             */
            class OPAAX_API OpaaxVulkanRenderer final : public OPAAX::IOpaaxRendererContext
            {
                //-----------------------------------------------------------------
                // Members
                //-----------------------------------------------------------------
                /*---------------------------- PRIVATE ----------------------------*/
                private:
                VkExtent2D                  m_windowExtent{ 1280 , 720 };
                
                VkSurfaceKHR                m_vkSurface                 = VK_NULL_HANDLE;
                VkInstance                  m_vkInstance                = VK_NULL_HANDLE;
                VkDebugUtilsMessengerEXT    m_vkDebugMessenger          = VK_NULL_HANDLE;
                VkPhysicalDevice            m_vkPhysicalDevice          = VK_NULL_HANDLE;
                VkDevice                    m_vkDevice                  = VK_NULL_HANDLE;
                VkSwapchainKHR              m_vkSwapchain               = VK_NULL_HANDLE;
                VkQueue                     m_vkGraphicsQueue           = VK_NULL_HANDLE;
                //Immediate submit structures
                VkFence                     m_immediateFence            = VK_NULL_HANDLE;
                VkCommandBuffer             m_immediateCommandBuffer    = VK_NULL_HANDLE;
                VkCommandPool               m_immediateCommandPool      = VK_NULL_HANDLE;
                
                VkFormat                    m_vkSwapchainImageFormat;

                VecVkImg                    m_vkSwapchainImages;
                VecVkImgView                m_vkSwapchainImageViews;
                VkExtent2D                  m_vkSwapchainExtent;

                UInt32                      m_graphicsQueueFamily{0};

                OpaaxVKFrameData            m_framesData[VULKAN_CONST::MAX_FRAMES_IN_FLIGHT];
                
                Int32                       m_frameNumber{0};

                OpaaxDeletionQueue          m_mainDeletionQueue;

                VmaAllocator                m_vmaAllocator;

                //draw resources
                OpaaxVKAllocatedImage       m_drawImage;
                VkExtent2D                  m_drawExtent;

                OpaaxVKDescriptorAllocator  m_globalDescriptorAllocator;

                VkDescriptorSet             m_vkDrawImageDescriptors;
                VkDescriptorSetLayout       m_vkDrawImageDescriptorLayout;

                //VkPipeline                  m_gradientPipeline;
                VkPipelineLayout            m_gradientPipelineLayout;

                std::vector<OpaaxVKComputeEffect>   m_backgroundEffects;
                Int32                               m_currentBackgroundEffect{0};

                //-----------------------------------------------------------------
                // CTOR - DTOR
                //-----------------------------------------------------------------
                /*---------------------------- PUBLIC ----------------------------*/
            public:
                explicit OpaaxVulkanRenderer();
                ~OpaaxVulkanRenderer() override;

                //-----------------------------------------------------------------
                // Functions
                //-----------------------------------------------------------------
                /*---------------------------- PRIVATE ----------------------------*/
            private:
                /**
                 * Initializes the Vulkan context by creating the Vulkan instance, validation layers (if enabled),
                 * Vulkan surface, and Vulkan device. This method leverages vk-bootstrap to streamline the setup process.
                 *
                 * The initialization includes:
                 *   - Building the Vulkan instance with the application name and enabling debug callbacks.
                 *   - Setting up validation layers and debug messenger (if validation layers are enabled).
                 *   - Retrieving and enabling the required Vulkan extensions.
                 *   - Creating the Vulkan surface required for rendering.
                 *   - Selecting a suitable physical GPU that supports Vulkan 1.3 with required features.
                 *   - Retrieving and setting Vulkan 1.3 and Vulkan 1.2 features on the physical device.
                 *   - Creating the logical Vulkan device for GPU operations.
                 *
                 * Logs details about Vulkan setup, including GPU selection, for debugging and tracing purposes.
                 *
                 * This method must be called before using any Vulkan-dependent functions in the renderer.
                 *
                 * @throws std::runtime_error If Vulkan initialization fails at any stage, including instance creation,
                 *         surface creation, GPU selection, or logical device creation.
                 */
                void InitVulkanBootStrap();
                /**
                 * Initializes the Vulkan Memory Allocator (VMA) for efficient GPU memory management.
                 *
                 * This method sets up a VMA allocator instance by providing necessary Vulkan objects such as
                 * the physical device, logical device, and Vulkan instance. It also applies specific configuration
                 * flags, including buffer device address support for advanced Vulkan memory operations.
                 *
                 * The created allocator is responsible for handling allocations and deallocations of GPU memory
                 * resources such as buffers and images, providing ease of use and optimal memory usage.
                 *
                 * Additionally, a clean-up function for destroying the VMA allocator is added to the main
                 * deletion queue to ensure proper resource management during the renderer's shutdown process.
                 */
                void InitVMAAllocator();
                /**
                 * Creates the Vulkan rendering surface required for the Vulkan instance.
                 * This surface acts as the bridge between the Vulkan API and the platform's native
                 * window system, enabling Vulkan to render graphical content to the screen.
                 *
                 * This method utilizes SDL_Vulkan_CreateSurface to create the surface, binding it
                 * to the Vulkan instance and the native window provided by the Opaax platform.
                 *
                 * Throws a runtime exception if surface creation fails.
                 *
                 * Key operations performed:
                 *   - Retrieves the native window associated with the rendering context.
                 *   - Invokes SDL to create the Vulkan-compatible surface.
                 *   - Checks for Vulkan surface creation errors.
                 */
                void CreateVulkanSurface();
                /**
                 * Initializes the Vulkan swapchain and its associated resources.
                 *
                 * This method sets up the Vulkan swapchain parameters and allocates the necessary
                 * GPU resources required for rendering. It ensures the rendering output aligns
                 * with the current window's dimensions and sets up an image for drawing operations.
                 *
                 * Functionality includes:
                 *   - Creating a Vulkan swapchain with dimensions matching the window's extents.
                 *   - Configuring a draw image with a hardcoded format of VK_FORMAT_R16G16B16A16_SFLOAT.
                 *   - Defining image usage flags suitable for transfer, storage, and rendering operations.
                 *   - Allocating GPU-local memory for the draw image using Vulkan Memory Allocator (VMA).
                 *   - Creating an image view for the draw image to be used in rendering pipelines.
                 *   - Adding cleanup tasks for the allocated resources to the deletion queue.
                 *
                 * This function should be called during the renderer initialization process
                 * to prepare the rendering pipeline's foundational components.
                 */
                void InitVulkanSwapchain();
                /**
                 * Initializes Vulkan command pools and command buffers required for rendering and immediate command submissions.
                 *
                 * This method sets up the Vulkan command infrastructure for the renderer by performing the following actions:
                 *   - Creates command pools for managing command buffers for each frame in flight, enabling reset capabilities
                 *     using VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT.
                 *   - Allocates main command buffers for rendering operations linked to each frame's command pool.
                 *   - Establishes an additional command pool and buffer for immediate command submissions that are used for
                 *     execution outside the standard rendering cycle.
                 *   - Registers cleanup functions in the deletion queue to ensure proper destruction of Vulkan objects associated
                 *     with command pools during cleanup.
                 *
                 * Command pools and buffers are initialized in a way to optimize memory usage and execution efficiency for
                 * multi-frame rendering pipelines while maintaining support for immediate GPU tasks.
                 */
                void InitVulkanCommands();
                /**
                 * Initializes Vulkan synchronization objects required for ensuring proper GPU execution and frame rendering order.
                 *
                 * This function creates the necessary synchronization primitives used for coordinating GPU operations,
                 * including:
                 *   - Fences for tracking frame rendering completion.
                 *   - Semaphores for synchronizing rendering with the swapchain and between GPU stages.
                 *
                 * Details include:
                 *   - Creation of one fence per frame to control when the GPU has completed rendering.
                 *   - Creation of two semaphores per frame to synchronize between the render pass and the swapchain operations.
                 *   - An additional immediate fence is created to enable the immediate submission mechanism (for imgui mainly).
                 *   - All resources created are integrated into the frame data structures for multi-frame operations.
                 *   - Proper destruction of the immediate fence is registered in the main deletion queue to ensure cleanup.
                 *
                 * This function assumes that the Vulkan device has already been initialized (`m_vkDevice`) and the constants
                 * such as `MAX_FRAMES_IN_FLIGHT` are appropriately defined.
                 */
                void InitVulkanSyncs();
                /**
                 * Configures and initializes the descriptor sets, layouts, and pools required for rendering operations.
                 * This method is responsible for setting up GPU resource bindings, enabling shaders to access the
                 * necessary uniform buffers, texture samplers, and other resources during rendering.
                 *
                 * The initialization process typically includes:
                 *   - Creation of descriptor pool for managing descriptor allocations.
                 *   - Definition of descriptor set layouts specifying resource bindings for shaders.
                 *   - Allocation and binding of descriptor sets to resources such as uniform buffers and textures.
                 *   - Updating descriptor sets with the correct resource information for rendering.
                 *
                 * This function ensures that the rendering pipeline has access to the required data and resources
                 * for proper execution and facilitates efficient resource management during the rendering process.
                 */
                void InitDescriptors();
                /**
                 * Initializes the Vulkan rendering pipelines required for rendering operations.
                 *
                 * This method is responsible for setting up and configuring the various graphics
                 * and compute pipelines that the renderer uses. Pipelines are essential in defining
                 * how rendering and compute tasks are executed on the GPU.
                 *
                 * Functionality provided includes:
                 *   - Creation and configuration of graphics pipelines for scene rendering.
                 *   - Setup of compute pipelines if required by specific rendering tasks.
                 *   - Management of pipeline configuration states such as shaders, fixed-function settings, and render passes.
                 *   - Ensuring compatibility with Vulkan's pipeline caching mechanism to optimize performance.
                 *
                 * This method primarily delegates and initializes specific sets of pipelines, tailored for the
                 * requirements of the engine.
                 */
                void InitPipelines();
                /**
                 * Initializes Vulkan compute pipelines for background effects within the renderer.
                 *
                 * This method sets up the necessary Vulkan pipeline layouts, manages push constant ranges,
                 * loads compute shaders, and creates compute pipelines for specified background effects.
                 * It includes:
                 *   - Configuration of pipeline layout using descriptor sets and push constants.
                 *   - Loading and initializing compute shader modules from SPIR-V files.
                 *   - Creation of compute pipelines for background effects such as gradient color and sky simulation.
                 *   - Default initialization of data parameters for each background effect.
                 *   - Cleanup of shader modules after pipeline creation.
                 *   - Storage of initialized compute pipelines in the internal background effect array for future use.
                 *
                 * The compute pipelines created here are specialized for rendering dynamic background elements
                 * in the OpaaxVulkanRenderer.
                 */
                void InitBackgroundPipelines();
                /**
                 * Initializes the ImGui interface with Vulkan-specific implementations for rendering debugging tools and UI.
                 *
                 * This method sets up the connection between ImGui and the Vulkan renderer, enabling the display and interaction of
                 * ImGui components within the Vulkan rendering context. It is responsible for:
                 *   - Acquiring the native SDL window handle to integrate ImGui.
                 *   - Initializing the Vulkan-specific ImGui implementation, connecting it to the Vulkan instance, physical device,
                 *     logical device, graphics queue, and swapchain image format used by the renderer.
                 *
                 * This function must be called after the Vulkan renderer has been initialized to ensure all Vulkan components are
                 * ready for use by ImGui.
                 */
                void InitImgui();

                /**
                 * Creates and configures a Vulkan swapchain for the rendering context.
                 * This method establishes a new swapchain with the specified dimensions,
                 * setting the desired format, color space, present mode, and usage flags.
                 * It also retrieves and stores the swapchain images and associated image views.
                 *
                 * @param Width The width of the swapchain surface in pixels.
                 * @param Height The height of the swapchain surface in pixels.
                 */
                void CreateSwapchain(UInt32 Width, UInt32 Height);
             
                void DrawBackground(VkCommandBuffer CommandBuffer);

                /**
                 * Destroys the Vulkan swapchain and its associated resources.
                 *
                 * This method is responsible for cleaning up all the resources related to the swapchain,
                 * such as the swapchain itself and any image views created for the swapchain images.
                 * It ensures that Vulkan resources are properly released to avoid memory leaks.
                 *
                 * The destruction process includes:
                 *   - Destroying the Vulkan swapchain using vkDestroySwapchainKHR.
                 *   - Iterating through and destroying all associated image views using vkDestroyImageView.
                 *
                 * This method is typically called during application shutdown, prior to recreation of
                 * a new swapchain during a window resize, or when the swapchain needs reconfiguration.
                 */
                void DestroySwapchain();

                /**
                 * Executes a user-provided function on a Vulkan command buffer through an immediate submission process.
                 * This method is designed for single-use command buffers and ensures the associated GPU operations
                 * are fully completed before returning.
                 *
                 * One way to improve it, would be to run it on a different queue than the graphics queue,
                 * and that way we could overlap the execution from this with the main render loop.
                 *
                 * @param Function Function object or lambda that defines the commands to be executed on the command buffer.
                 *        The provided command buffer is passed as a parameter to the function.
                 */
                void ImmediateSubmit(OPSTDFunc<void(VkCommandBuffer CommandBuffer)>&& Function);

                /**
                 * Renders ImGui UI elements using Vulkan commands and outputs them to a specified target image view.
                 * This method handles Vulkan rendering setup, including defining the rendering attachments
                 * and invoking the ImGui render commands using the provided Vulkan command buffer.
                 *
                 * @param CommandBuffer The Vulkan command buffer used for recording the rendering commands.
                 * @param TargetImageView The target image view where the UI will be rendered. This represents
                 *                        the image to be used as the color attachment for rendering.
                 */
                void DrawImgui(VkCommandBuffer CommandBuffer,  VkImageView TargetImageView);

                /*---------------------------- GETTER  ----------------------------*/
                /**
                 * Retrieves the SDL window flags used to configure the window for Vulkan rendering.
                 * These flags ensure that the window is compatible with Vulkan API requirements.
                 *
                 * @return SDL_WindowFlags indicating the Vulkan-specific configuration for the SDL window.
                 */
                FORCEINLINE SDL_WindowFlags   GetWindowFlags() override { return (SDL_WindowFlags)(SDL_WINDOW_VULKAN); }
                FORCEINLINE OpaaxVKFrameData& GetCurrentFrameData()     { return m_framesData[m_frameNumber % VULKAN_CONST::MAX_FRAMES_IN_FLIGHT]; }

                //-----------------------------------------------------------------
                // Override
                //-----------------------------------------------------------------
                /*---------------------------- PUBLIC ----------------------------*/
            public:
                bool Initialize(OpaaxWindow& OpaaxWindow)   override;
                void Resize()       override;
                void DrawImgui()    override;
                void RenderFrame()  override;
                void Shutdown()     override;
            };
        }
    }
}
