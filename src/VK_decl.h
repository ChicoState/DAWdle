#pragma once
#include "DrillLib.h"
// The *_decl files are really part of their main files, just including
// some declarations for things because the C++ file dependency system sucks.
#define VK_NO_PROTOTYPES
#define VK_NO_STDINT_H
#define VK_NO_STDDEF_H
#pragma warning(push, 0)
#include "../external/vulkan/vulkan.h"
// From vulkan_win32.h. I didn't want to include that file directly because it includes windows.h and I may want to remove that include eventually.
#define VK_KHR_WIN32_SURFACE_EXTENSION_NAME "VK_KHR_win32_surface"
typedef VkFlags VkWin32SurfaceCreateFlagsKHR;
typedef struct VkWin32SurfaceCreateInfoKHR {
	VkStructureType                 sType;
	const void* pNext;
	VkWin32SurfaceCreateFlagsKHR    flags;
	HINSTANCE                       hinstance;
	HWND                            hwnd;
} VkWin32SurfaceCreateInfoKHR;

typedef VkResult(VKAPI_PTR* PFN_vkCreateWin32SurfaceKHR)(VkInstance instance, const VkWin32SurfaceCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSurfaceKHR* pSurface);
typedef VkBool32(VKAPI_PTR* PFN_vkGetPhysicalDeviceWin32PresentationSupportKHR)(VkPhysicalDevice physicalDevice, uint32_t queueFamilyIndex);
#pragma warning(pop)
#include "VKStaging_decl.h"

#define CHK_VK(cmd) { VkResult chkVK_Result = cmd; if(chkVK_Result != VK_SUCCESS){ ::VK::vulkan_failure(chkVK_Result, ""); } }
#define CHK_VK_NOT_NULL(cmd, msg) if((cmd) == nullptr){ ::VK::vulkan_failure(VK_SUCCESS, msg); }
#define VK_DEBUG 0
namespace VK {
#define VK_INSTANCE_FUNCTIONS X(vkGetDeviceProcAddr)\
	X(vkGetDeviceQueue)\
	X(vkGetPhysicalDeviceQueueFamilyProperties)\
	X(vkGetPhysicalDeviceSurfaceSupportKHR)\
	X(vkDestroyInstance)\
	X(vkDestroyDevice)\
	X(vkGetPhysicalDeviceMemoryProperties)\
	X(vkGetPhysicalDeviceProperties)\
	X(vkGetPhysicalDeviceSurfaceCapabilitiesKHR)\
	X(vkGetPhysicalDeviceWin32PresentationSupportKHR)\
	X(vkGetPhysicalDeviceSurfacePresentModesKHR)\
	X(vkGetPhysicalDeviceSurfaceFormatsKHR)\
	X(vkCreateWin32SurfaceKHR)\
	X(vkDestroySurfaceKHR)\
	X(vkEnumeratePhysicalDevices)\
	X(vkGetPhysicalDeviceProperties2)\
	X(vkGetPhysicalDeviceFeatures2)\
	X(vkEnumerateDeviceExtensionProperties)\
	X(vkCreateDevice)

#if VK_DEBUG != 0
#define VK_DEBUG_FUNCTIONS X(vkCreateDebugUtilsMessengerEXT)\
	X(vkDestroyDebugUtilsMessengerEXT)
#else
#define VK_DEBUG_FUNCTIONS
#endif

#define VK_DEVICE_FUNCTIONS X(vkCmdBeginRenderPass)\
	X(vkCmdEndRenderPass)\
	X(vkCreateCommandPool)\
	X(vkAllocateCommandBuffers)\
	X(vkCreateRenderPass)\
	X(vkDestroyRenderPass)\
	X(vkCreateFramebuffer)\
	X(vkDestroyFramebuffer)\
	X(vkBeginCommandBuffer)\
	X(vkEndCommandBuffer)\
	X(vkQueueSubmit)\
	X(vkDeviceWaitIdle)\
	X(vkCreateImageView)\
	X(vkDestroyImageView)\
	X(vkResetCommandPool)\
	X(vkQueueWaitIdle)\
	X(vkCmdBindPipeline)\
	X(vkCmdDraw)\
	X(vkDestroyPipeline)\
	X(vkCreateGraphicsPipelines)\
	X(vkCreatePipelineLayout)\
	X(vkDestroyPipelineLayout)\
	X(vkCreateShaderModule)\
	X(vkDestroyShaderModule)\
	X(vkCmdSetViewport)\
	X(vkCmdSetScissor)\
	X(vkCmdPushConstants)\
	X(vkCreateDescriptorPool)\
	X(vkCreateDescriptorSetLayout)\
	X(vkAllocateMemory)\
	X(vkCreateBuffer)\
	X(vkCreateBufferView)\
	X(vkCmdBindVertexBuffers)\
	X(vkCmdBindIndexBuffer)\
	X(vkDestroyBuffer)\
	X(vkFreeMemory)\
	X(vkMapMemory)\
	X(vkUnmapMemory)\
	X(vkFlushMappedMemoryRanges)\
	X(vkCmdCopyBuffer)\
	X(vkCmdCopyImage)\
	X(vkCmdCopyBufferToImage)\
	X(vkCmdCopyImageToBuffer)\
	X(vkCmdPipelineBarrier)\
	X(vkBindBufferMemory)\
	X(vkDestroyCommandPool)\
	X(vkCreateSemaphore)\
	X(vkCreateFence)\
	X(vkWaitForFences)\
	X(vkResetFences)\
	X(vkCreateComputePipelines)\
	X(vkCmdDrawIndexed)\
	X(vkCreateImage)\
	X(vkDestroyImage)\
	X(vkBindImageMemory)\
	X(vkGetBufferMemoryRequirements)\
	X(vkGetImageMemoryRequirements)\
	X(vkCmdBlitImage)\
	X(vkAllocateDescriptorSets)\
	X(vkDestroyDescriptorSetLayout)\
	X(vkDestroyDescriptorPool)\
	X(vkCmdBindDescriptorSets)\
	X(vkCmdDispatch)\
	X(vkUpdateDescriptorSets)\
	X(vkQueuePresentKHR)\
	X(vkGetFenceStatus)\
	X(vkAcquireNextImageKHR)\
	X(vkCreateSwapchainKHR)\
	X(vkDestroySwapchainKHR)\
	X(vkGetSwapchainImagesKHR)\
	X(vkDestroyFence)\
	X(vkDestroySemaphore)\
	X(vkCreateSampler)\
	X(vkDestroySampler)\
	X(vkGetBufferDeviceAddress)

#define X(name) extern PFN_##name name;
VK_INSTANCE_FUNCTIONS
VK_DEBUG_FUNCTIONS
VK_DEVICE_FUNCTIONS
#undef X

const U32 FRAMES_IN_FLIGHT = 2;

extern U32 currentFrameInFlight;

extern VkInstance vkInstance;
extern VkPhysicalDevice physicalDevice;
extern VkDevice logicalDevice;

struct FramebufferAttachment {
	VkDeviceMemory memory;
	VkImage image;
	VkImageView imageView;
	VkFormat imageFormat;
	VkImageUsageFlags imageUsage;
	VkImageAspectFlags imageAspectMask;
	B32 ownsImage;
};

struct Framebuffer {
	static constexpr U32 MAX_FRAMEBUFFER_ATTACHMENTS = 4;
	VkFramebuffer framebuffer;
	VkRenderPass renderPass;
	U32 framebufferWidth;
	U32 framebufferHeight;
	FramebufferAttachment attachments[MAX_FRAMEBUFFER_ATTACHMENTS];
	U32 attachmentCount;
	B32 addedToFreeList;

	Framebuffer& set_default();
	Framebuffer& render_pass(VkRenderPass pass);
	Framebuffer& dimensions(U32 width, U32 height);
	Framebuffer& new_attachment(VkFormat imageFormat, VkImageUsageFlags usage, VkImageAspectFlags aspectMask);
	Framebuffer& existing_attachment(VkImageView view);
	Framebuffer& build();
	void destroy();
};

struct DescriptorSet {
	// I'm just going to always create these together.
	// I won't have that many descriptor sets because of the bindless architecture, and this simplifies a lot
	VkDescriptorSetLayout setLayout;
	VkDescriptorSet descriptorSet;
	VkDescriptorPool descriptorPool;

	static constexpr U32 MAX_LAYOUT_BINDINGS = 16;
	VkDescriptorSetLayoutBinding bindings[MAX_LAYOUT_BINDINGS];
	VkDescriptorBindingFlags bindingFlags[MAX_LAYOUT_BINDINGS];
	U32 bindingCount;
	U32 variableDescriptorCountMax;
	B32 updateAfterBind;

	DescriptorSet& init();

	DescriptorSet& basic_binding(U32 bindingIndex, VkShaderStageFlags stageFlags, VkDescriptorType descriptorType, U32 descriptorCount, VkDescriptorBindingFlags flags);
	DescriptorSet& storage_buffer(U32 bindingIndex, VkShaderStageFlags stageFlags);
	DescriptorSet& uniform_buffer(U32 bindingIndex, VkShaderStageFlags stageFlags);
	DescriptorSet& sampler(U32 bindingIndex, VkShaderStageFlags stageFlags);
	DescriptorSet& texture_array(U32 bindingIndex, VkShaderStageFlags stageFlags, U32 maxArraySize, U32 arraySize);

	void build();

	// Should only be called before rendering anything in the current frame
	void change_dynamic_array_length(U32 newDescriptorCount);

	U32 current_dynamic_array_length();
};

extern Framebuffer mainFramebuffer;

extern U32 hostMemoryTypeIndex;
extern U32 deviceMemoryTypeIndex;
extern VkMemoryPropertyFlags hostMemoryFlags;
extern VkMemoryPropertyFlags deviceMemoryFlags;

extern VkPhysicalDeviceProperties physicalDeviceProperties;

extern U32 graphicsFamily;
extern U32 transferFamily;
extern U32 computeFamily;

extern VkQueue graphicsQueue;
extern VkQueue transferQueue;
extern VkQueue computeQueue;

void vulkan_failure(VkResult result, const char* msg);

}