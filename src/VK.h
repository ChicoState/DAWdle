#pragma once
#include "VK_decl.h"
#include "VKStaging.h"
#include "Win32.h"
#include "Textures_decl.h"
#include "DynamicVertexBuffer_decl.h"


namespace VK {
#if VK_DEBUG
#define VK_ENABLE_VALIDATION_LAYERS 1
#else
#define VK_ENABLE_VALIDATION_LAYERS 0
#endif

const char* ENABLED_VALIDATION_LAYERS[]{ "VK_LAYER_KHRONOS_validation", "VK_LAYER_KHRONOS_synchronization2" };

const char* INSTANCE_EXTENSIONS[]{
#if VK_DEBUG != 0	
VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
#endif
VK_KHR_SURFACE_EXTENSION_NAME,
VK_KHR_WIN32_SURFACE_EXTENSION_NAME
};

const char* DEVICE_EXTENSIONS[]{
#if VK_DEBUG != 0	
#endif
VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr;
PFN_vkCreateInstance vkCreateInstance;
#define X(name) PFN_##name name;
VK_INSTANCE_FUNCTIONS
VK_DEBUG_FUNCTIONS
VK_DEVICE_FUNCTIONS
#undef X

VkInstance vkInstance;
VkDebugUtilsMessengerEXT messenger;
VkPhysicalDevice physicalDevice;
VkDevice logicalDevice;

VkPhysicalDeviceProperties physicalDeviceProperties;

U32 graphicsFamily;
U32 transferFamily;
U32 computeFamily;
#define FAMILY_PRESENT_BIT_GRAPHICS (1 << 0)
#define FAMILY_PRESENT_BIT_TRANSFER (1 << 1)
#define FAMILY_PRESENT_BIT_COMPUTE (1 << 2)
U32 familyPresentBits;

U32 hostMemoryTypeIndex;
U32 deviceMemoryTypeIndex;
VkMemoryPropertyFlags hostMemoryFlags;
VkMemoryPropertyFlags deviceMemoryFlags;

VkQueue graphicsQueue;
VkQueue transferQueue;
VkQueue computeQueue;

U32 currentFrameInFlight;

VKStaging::GPUUploadStager graphicsStager;

Framebuffer mainFramebuffer;

VkCommandPool graphicsCommandPools[FRAMES_IN_FLIGHT];
// These two get swapped each frame so we don't overwrite one while it's in use
VkCommandBuffer graphicsCommandBuffer;
VkCommandBuffer inFlightGraphicsCommandBuffer;

// For GPU operations to not conflict with previous frame results
VkSemaphore renderFinishedSemaphores[FRAMES_IN_FLIGHT];
// For CPU operations to not overwrite data in use by the GPU
VkFence renderFinishedFences[FRAMES_IN_FLIGHT];

VkRenderPass mainRenderPass;

VkSampler linearSampler;

DescriptorSet drawDataDescriptorSet;

const U32 UI_RENDER_FLAG_MSDF = 1 << 0;
VkPipelineLayout uiPipelineLayout;
VkPipeline uiPipeline;

struct DedicatedBuffer {
	VkDeviceMemory memory;
	VkBuffer buffer;
	void* mapping;
	VkDeviceAddress gpuAddress;
	U64 capacity;

	void create(U64 size, VkBufferUsageFlags usage, U32 memoryTypeIndex) {
		capacity = size;
		VkBufferCreateInfo bufferInfo{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
		bufferInfo.flags = 0;
		bufferInfo.size = size;
		bufferInfo.usage = usage;
		bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		bufferInfo.queueFamilyIndexCount = 0;
		bufferInfo.pQueueFamilyIndices = nullptr;
		CHK_VK(vkCreateBuffer(logicalDevice, &bufferInfo, nullptr, &buffer));

		VkMemoryRequirements memoryRequirements;
		vkGetBufferMemoryRequirements(logicalDevice, buffer, &memoryRequirements);
		if (!(memoryRequirements.memoryTypeBits & 1 << memoryTypeIndex)) {
			abort("Could not create buffer in correct memory type");
		}

		VkMemoryAllocateInfo allocInfo{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
		allocInfo.allocationSize = memoryRequirements.size;
		allocInfo.memoryTypeIndex = memoryTypeIndex;
		VkMemoryAllocateFlagsInfo allocFlags{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO };
		if (usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) {
			allocFlags.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
		} else {
			allocFlags.flags = 0;
		}
		allocFlags.deviceMask = 0;
		allocInfo.pNext = &allocFlags;
		VkMemoryDedicatedAllocateInfo dedicatedAllocInfo{ VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO };
		dedicatedAllocInfo.buffer = buffer;
		allocFlags.pNext = &dedicatedAllocInfo;
		CHK_VK(vkAllocateMemory(logicalDevice, &allocInfo, nullptr, &memory));
		CHK_VK(vkBindBufferMemory(logicalDevice, buffer, memory, 0));

		if (usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) {
			VkBufferDeviceAddressInfo addressInfo{ VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };
			addressInfo.buffer = buffer;
			gpuAddress = vkGetBufferDeviceAddress(logicalDevice, &addressInfo);
		} else {
			gpuAddress = 0;
		}

		if (memoryTypeIndex == hostMemoryTypeIndex) {
			CHK_VK(vkMapMemory(logicalDevice, memory, 0, capacity, 0, reinterpret_cast<void**>(&mapping)));
		}
	}
	void destroy() {
		if (memory != VK_NULL_HANDLE) {
			if (mapping) {
				vkUnmapMemory(logicalDevice, memory);
			}
			vkDestroyBuffer(logicalDevice, buffer, nullptr);
			vkFreeMemory(logicalDevice, memory, nullptr);
			memory = VK_NULL_HANDLE;
			buffer = VK_NULL_HANDLE;
			mapping = nullptr;
			gpuAddress = 0;
			capacity = 0;
		}
	}
	void invalidate_mapped(VkDeviceSize offset = 0, VkDeviceSize size = VK_WHOLE_SIZE) {
		if (mapping && !(VK::hostMemoryTypeIndex & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
			VkMappedMemoryRange memoryInvalidateRange{ VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE };
			memoryInvalidateRange.memory = memory;
			memoryInvalidateRange.offset = offset;
			memoryInvalidateRange.size = size;
			CHK_VK(VK::vkFlushMappedMemoryRanges(VK::logicalDevice, 1, &memoryInvalidateRange));
		}
	}
};

// This struct simply groups related data, there should not be more than one
struct DesktopSwapchainData {
	VkSurfaceKHR surface;
	VkSwapchainKHR swapchain;
	VkImage* swapchainImages;
	VkSemaphore swapchainAcquireSemaphores[FRAMES_IN_FLIGHT];
	U32 swapchainImageCount;
	U32 swapchainImageIdx;
	U32 width;
	U32 height;

	void make_surface() {
		VkWin32SurfaceCreateInfoKHR surfaceInfo{ VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR };
		surfaceInfo.flags = 0;
		surfaceInfo.hinstance = Win32::instance;
		surfaceInfo.hwnd = Win32::window;
		CHK_VK(vkCreateWin32SurfaceKHR(vkInstance, &surfaceInfo, nullptr, &surface));
	}

	void create(I32 newWidth, I32 newHeight, VkSwapchainKHR oldSwapchain) {
		MemoryArena& stackArena = get_scratch_arena();
		U64 stackArenaFrame0 = stackArena.stackPtr;
		if (newWidth <= 0 || newHeight <= 0 || vkGetPhysicalDeviceWin32PresentationSupportKHR(physicalDevice, graphicsFamily) == VK_FALSE) {
			swapchain = VK_NULL_HANDLE;
			width = 0;
			height = 0;
		} else {
			if (surface == VK_NULL_HANDLE) {
				make_surface();
			}

			VkSurfaceCapabilitiesKHR surfaceCaps;
			CHK_VK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCaps));
			if (surfaceCaps.currentExtent.width != U32_MAX) {
				width = surfaceCaps.currentExtent.width;
				height = surfaceCaps.currentExtent.height;
			} else {
				width = clamp(U32(newWidth), surfaceCaps.minImageExtent.width, surfaceCaps.maxImageExtent.height);
				height = clamp(U32(newHeight), surfaceCaps.minImageExtent.height, surfaceCaps.maxImageExtent.height);
			}

			if (width == 0 || height == 0) {
				swapchain = VK_NULL_HANDLE;
			} else {
				U32 surfaceFormatCount;
				CHK_VK(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &surfaceFormatCount, nullptr));
				VkSurfaceFormatKHR* surfaceFormatsSupported = stackArena.alloc<VkSurfaceFormatKHR>(surfaceFormatCount);
				CHK_VK(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &surfaceFormatCount, surfaceFormatsSupported));
				VkSurfaceFormatKHR surfaceFormat = surfaceFormatsSupported[0];
				for (U32 i = 0; i < surfaceFormatCount; i++) {
					if (surfaceFormatsSupported[i].format == VK_FORMAT_B8G8R8A8_SRGB && surfaceFormatsSupported[i].colorSpace == VK_COLORSPACE_SRGB_NONLINEAR_KHR) {
						surfaceFormat = surfaceFormatsSupported[i];
						break;
					}
				}

				VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
				/*U32 presentModeCount;
				CHK_VK(vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, nullptr));
				VkPresentModeKHR* presentModesSupported = stackArena.alloc<VkPresentModeKHR>(presentModeCount);
				CHK_VK(vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, presentModesSupported));
				for (U32 i = 0; i < presentModeCount; i++) {
					if (presentModesSupported[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
						presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
						break;
					}
				}*/

				VkSwapchainCreateInfoKHR swapchainCreateInfo{ VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
				swapchainCreateInfo.flags = 0;
				swapchainCreateInfo.surface = surface;
				swapchainCreateInfo.minImageCount = clamp(2u, surfaceCaps.minImageCount, surfaceCaps.maxImageCount == 0 ? U32_MAX : surfaceCaps.maxImageCount);
				swapchainCreateInfo.imageFormat = surfaceFormat.format;
				swapchainCreateInfo.imageColorSpace = surfaceFormat.colorSpace;
				swapchainCreateInfo.imageExtent.width = width;
				swapchainCreateInfo.imageExtent.height = height;
				swapchainCreateInfo.imageArrayLayers = 1;
				swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
				swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
				swapchainCreateInfo.queueFamilyIndexCount = 0;
				swapchainCreateInfo.pQueueFamilyIndices = nullptr;
				swapchainCreateInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
				swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
				swapchainCreateInfo.presentMode = presentMode;
				swapchainCreateInfo.clipped = VK_TRUE;
				swapchainCreateInfo.oldSwapchain = oldSwapchain;
				CHK_VK(vkCreateSwapchainKHR(logicalDevice, &swapchainCreateInfo, VK_NULL_HANDLE, &swapchain));

				U32 newSwapchainImageCount;
				CHK_VK(vkGetSwapchainImagesKHR(logicalDevice, swapchain, &newSwapchainImageCount, nullptr));
				if (newSwapchainImageCount > swapchainImageCount) {
					swapchainImages = globalArena.alloc_and_commit<VkImage>(newSwapchainImageCount);
				}
				swapchainImageCount = newSwapchainImageCount;
				CHK_VK(vkGetSwapchainImagesKHR(logicalDevice, swapchain, &swapchainImageCount, swapchainImages));

				for (VkSemaphore& sem : swapchainAcquireSemaphores) {
					VkSemaphoreCreateInfo semaphoreInfo{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
					semaphoreInfo.flags = 0;
					CHK_VK(vkCreateSemaphore(logicalDevice, &semaphoreInfo, nullptr, &sem));
				}
			}
		}
		stackArena.stackPtr = stackArenaFrame0;
	}

	void resize(I32 newWidth, I32 newHeight) {
		// Heavy sync, but this is only expected to run when the user resizes the desktop window anyway
		CHK_VK(vkDeviceWaitIdle(logicalDevice));
		VkSwapchainKHR oldSwapchain = swapchain;
		if (oldSwapchain != VK_NULL_HANDLE) {
			// We don't have to destroy these, but it makes the code a little simpler, and like I said, it doesn't matter if we're a little inefficient here
			for (VkSemaphore sem : swapchainAcquireSemaphores) {
				vkDestroySemaphore(logicalDevice, sem, nullptr);
			}
			swapchain = VK_NULL_HANDLE;
		}
		this->create(newWidth, newHeight, oldSwapchain);
		if (oldSwapchain != VK_NULL_HANDLE) {
			vkDestroySwapchainKHR(logicalDevice, oldSwapchain, nullptr);
		}
	}

	void destroy() {
		if (swapchain != VK_NULL_HANDLE) {
			for (VkSemaphore sem : swapchainAcquireSemaphores) {
				vkDestroySemaphore(logicalDevice, sem, nullptr);
			}
			vkDestroySwapchainKHR(logicalDevice, swapchain, nullptr);
			vkDestroySurfaceKHR(vkInstance, surface, nullptr);
		}
	}
} desktopSwapchainData;

struct {
	ArenaArrayList<DescriptorSet*> descriptorSets;
	ArenaArrayList<Framebuffer*> framebuffers;
	ArenaArrayList<VkPipelineLayout> pipelineLayouts;
	ArenaArrayList<VkPipeline> pipelines;
	ArenaArrayList<VkSampler> samplers;
} destroyLists;

struct {
	ArenaArrayList<VkDescriptorPool> descriptorPools;
} frameDestroyLists[FRAMES_IN_FLIGHT];

void vulkan_failure(VkResult result, const char* msg) {
	print("VK function failed! Error code: ");
	println_integer(result);
	print(msg);
	println();

	__debugbreak();
	ExitProcess(EXIT_FAILURE);
}

B32 load_first_vulkan_functions() {
	HMODULE vulkan = LoadLibraryA("vulkan-1.dll");
	if (!vulkan) {
		print("vulkan-1.dll not found! Perhaps upgrade your graphics drivers?\n");
		return false;
	}
	if ((vkGetInstanceProcAddr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(reinterpret_cast<void*>(GetProcAddress(vulkan, "vkGetInstanceProcAddr")))) == nullptr) {
		return false;
	}
	if ((vkCreateInstance = reinterpret_cast<PFN_vkCreateInstance>(reinterpret_cast<void*>(vkGetInstanceProcAddr(VK_NULL_HANDLE, "vkCreateInstance")))) == nullptr) {
		return false;
	}
	return true;
}

void load_instance_vulkan_functions() {
#define X(name) CHK_VK_NOT_NULL(name = reinterpret_cast<PFN_##name>(reinterpret_cast<void*>(vkGetInstanceProcAddr(vkInstance, #name))), #name);
	VK_INSTANCE_FUNCTIONS
	VK_DEBUG_FUNCTIONS
#undef X
}

void load_device_vulkan_functions() {
#define X(name) CHK_VK_NOT_NULL(name = reinterpret_cast<PFN_##name>(reinterpret_cast<void*>(vkGetDeviceProcAddr(logicalDevice, #name))), #name);
	VK_DEVICE_FUNCTIONS
		
#undef X
}

static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
	VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT messageType,
	const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
	void* pUserData) {

	if (messageSeverity > VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
		print("VK validation layer: ");
		print(pCallbackData->pMessage);
		print("\n");
	}

	// According to spec, I should always return VK_FALSE here, VK_TRUE is reserved for layers
	return VK_FALSE;
}

void recompile_modified_shaders(StrA shaderDir, StrA outputDir) {
	print("Recompiling modified shaders...\n");
	MemoryArena& stackArena = get_scratch_arena();
	B32 programCompileFailure = false;
	MEMORY_ARENA_FRAME(stackArena) {
		const char* shaderDirFilesCStr = stracat(stackArena, shaderDir, "*\0"sa).str;
		// Look for a timestamp file (file doesn't contain anything, but we can read the last recompile time from its metadata)
		U64 previousRecompTime = 0;
		HANDLE timestampFile = CreateFileA(stracat(stackArena, outputDir, "_timestamp\0"sa).str, GENERIC_WRITE | FILE_WRITE_ATTRIBUTES, FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if (timestampFile != INVALID_HANDLE_VALUE) {
			FILETIME lastModifiedTime;
			BOOL timeSuccess = GetFileTime(timestampFile, NULL, &lastModifiedTime, NULL);
			if (timeSuccess) {
				previousRecompTime = U64(lastModifiedTime.dwHighDateTime) << 32ull | U64(lastModifiedTime.dwLowDateTime);
			}
		} else {
			timestampFile = CreateFileA(stracat(stackArena, outputDir, "_timestamp\0"sa).str, GENERIC_WRITE | FILE_WRITE_ATTRIBUTES, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		}
		U64 maxAccessedTime = previousRecompTime;
		
		// Search through all files in the shader directory, if it's a shader and it was modified since the last recompile, add its directory to the list
		WIN32_FIND_DATAA findData;
		HANDLE findEntry = FindFirstFileA(shaderDirFilesCStr, &findData);
		if (findEntry != INVALID_HANDLE_VALUE) {
			do {
				if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
					continue;
				}
				U64 findTime = U64(findData.ftLastWriteTime.dwHighDateTime) << 32ull | U64(findData.ftLastWriteTime.dwLowDateTime);
				maxAccessedTime = max(maxAccessedTime, findTime);
				if (findTime <= previousRecompTime) {
					continue;
				}
				StrA fileName = StrA{ findData.cFileName, strlen(findData.cFileName) };
				StrA ext{};
				if (fileName.ends_with(".vert"sa)) {
					ext = ".vspv"sa;
				} else if (fileName.ends_with(".frag"sa)) {
					ext = ".fspv"sa;
				}
				if (ext.is_empty()) {
					continue;
				}
				// While I would love to use my own shading language for this and include the compiler directly, I simply do not have time to rewrite the compiler to bring it up to standard
				StrA nameNoExt = fileName.prefix(-I64(ext.length));
				StrA outputPath = stracat(stackArena, outputDir, nameNoExt, ext);
				StrA programToRun = stracat(stackArena, outputDir, "glslc.exe"sa);
				StrA programArgs = stracat(stackArena, "\""sa, shaderDir, fileName, "\" -g -O -o \""sa, outputPath, "\" --target-env=vulkan1.2"sa);
				println(stracat(stackArena, "Recompiling: "sa, shaderDir, fileName));
				U32 exitCode;
				if (!run_program_and_wait(&exitCode, programToRun, programArgs)) {
					print("Failed to run glslc\n");
				}
				programCompileFailure |= exitCode != 0;
			} while (FindNextFileA(findEntry, &findData));
			FindClose(findEntry);
		} else {
			print("Failed to iterate directory for shader recompilation\n");
		}

		// Close the timestamp file, a new last write time will be added to the file's metadata
		if (timestampFile != INVALID_HANDLE_VALUE) {
			if (!programCompileFailure) {
				FILETIME newAccessTime{};
				newAccessTime.dwLowDateTime = U32(maxAccessedTime);
				newAccessTime.dwHighDateTime = U32(maxAccessedTime >> 32ull);
				SetFileTime(timestampFile, nullptr, &newAccessTime, nullptr);
			}
			CloseHandle(timestampFile);
		}
	}
	if (programCompileFailure) {
		abort("Shader compilation failed");
	} else {
		print("Shader recompilation complete.\n");
	}
}

void create_render_targets();

void init_vulkan() {
	recompile_modified_shaders(".\\resources\\shaders\\"sa, ".\\resources\\shaders\\spirv\\"sa);

	if (!load_first_vulkan_functions()) {
		abort("Failed to load Vulkan Functions");
	}

	MemoryArena& stackArena = get_scratch_arena();

	/*u32 instanceExtensionCount{};
	CHK_XR(XR::xrGetVulkanInstanceExtensionsKHR(XR::xrInstance, XR::systemID, 0, &instanceExtensionCount, nullptr));
	char* xrRequiredExtensions = stackArena.alloc<char>(instanceExtensionCount + 1);
	CHK_XR(XR::xrGetVulkanInstanceExtensionsKHR(XR::xrInstance, XR::systemID, instanceExtensionCount, &instanceExtensionCount, xrRequiredExtensions));
	xrRequiredExtensions[instanceExtensionCount] = 0;*/

	ArenaArrayList<const char*> enabledLayers{ &stackArena };
#if VK_ENABLE_VALIDATION_LAYERS != 0
	enabledLayers.push_back_n(ENABLED_VALIDATION_LAYERS, ARRAY_COUNT(ENABLED_VALIDATION_LAYERS));
#endif
	ArenaArrayList<const char*> enabledExtensions{ &stackArena };
	enabledExtensions.push_back_n(INSTANCE_EXTENSIONS, ARRAY_COUNT(INSTANCE_EXTENSIONS));
	/*while(*xrRequiredExtensions){
		enabledExtensions.push_back(xrRequiredExtensions);
		while (*xrRequiredExtensions && *xrRequiredExtensions != ' ') {
			xrRequiredExtensions++;
		}
		if (*xrRequiredExtensions == ' ') {
			*xrRequiredExtensions++ = '\0';
		}
	}*/

	VkApplicationInfo applicationInfo{ VK_STRUCTURE_TYPE_APPLICATION_INFO };
	applicationInfo.pApplicationName = "Audio Programming";
	applicationInfo.applicationVersion = 1;
	applicationInfo.pEngineName = "DrillAudioEngine";
	applicationInfo.engineVersion = 1;
	applicationInfo.apiVersion = VK_API_VERSION_1_2;
	VkInstanceCreateInfo instanceCreateInfo{ VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
	instanceCreateInfo.flags = 0;
	instanceCreateInfo.pApplicationInfo = &applicationInfo;
	instanceCreateInfo.enabledLayerCount = enabledLayers.size;
	instanceCreateInfo.ppEnabledLayerNames = enabledLayers.data;
	instanceCreateInfo.enabledExtensionCount = enabledExtensions.size;
	instanceCreateInfo.ppEnabledExtensionNames = enabledExtensions.data;
#if VK_DEBUG != 0
	VkDebugUtilsMessengerCreateInfoEXT messengerCreateInfo{ VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
	messengerCreateInfo.messageSeverity =
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	messengerCreateInfo.messageType =
		VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	messengerCreateInfo.pfnUserCallback = debug_callback;
	instanceCreateInfo.pNext = &messengerCreateInfo;
#endif

	CHK_VK(vkCreateInstance(&instanceCreateInfo, nullptr, &vkInstance));

	load_instance_vulkan_functions();

#if VK_DEBUG != 0
	CHK_VK(vkCreateDebugUtilsMessengerEXT(vkInstance, &messengerCreateInfo, VK_NULL_HANDLE, &messenger));
#endif

	// Must be called before device selection, since we need to know if the device supports the surface
	desktopSwapchainData.make_surface();

	U32 physicalDeviceCount;
	CHK_VK(vkEnumeratePhysicalDevices(vkInstance, &physicalDeviceCount, nullptr));
	if (physicalDeviceCount == 0) {
		abort("No vulkan capable physical devices found\n");
	}
	VkPhysicalDevice* physicalDevices = stackArena.alloc<VkPhysicalDevice>(physicalDeviceCount);
	CHK_VK(vkEnumeratePhysicalDevices(vkInstance, &physicalDeviceCount, physicalDevices));

	U32 bestPhysicalDeviceScore = 0;
	physicalDevice = VK_NULL_HANDLE;
	for (VkPhysicalDevice* dev = physicalDevices; dev != physicalDevices + physicalDeviceCount; dev++) {
		U64 oldStack = stackArena.stackPtr;
		VkPhysicalDeviceProperties deviceProperties;
		vkGetPhysicalDeviceProperties(*dev, &deviceProperties);
		VkPhysicalDeviceSubgroupProperties subgroupProperties;
		subgroupProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES;
		subgroupProperties.pNext = VK_NULL_HANDLE;
		VkPhysicalDeviceProperties2 deviceProperties2;
		deviceProperties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
		deviceProperties2.pNext = &subgroupProperties;
		vkGetPhysicalDeviceProperties2(*dev, &deviceProperties2);
		VkPhysicalDeviceFeatures2 deviceFeatures2;
		deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
		VkPhysicalDeviceVulkan12Features deviceFeatures12;
		deviceFeatures12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
		deviceFeatures2.pNext = &deviceFeatures12;
		deviceFeatures12.pNext = nullptr;
		vkGetPhysicalDeviceFeatures2(*dev, &deviceFeatures2);
		VkPhysicalDeviceFeatures& deviceFeatures = deviceFeatures2.features;

		{ // Make sure all necessary queue families are supported
			U32 familyCount = 0;
			vkGetPhysicalDeviceQueueFamilyProperties(*dev, &familyCount, nullptr);
			VkQueueFamilyProperties* queueFamilyProperties = stackArena.alloc<VkQueueFamilyProperties>(familyCount);
			vkGetPhysicalDeviceQueueFamilyProperties(*dev, &familyCount, queueFamilyProperties);
			VkQueueFlags allQueueFlags = 0;
			for (VkQueueFamilyProperties* prop = queueFamilyProperties; prop != queueFamilyProperties + familyCount; prop++) {
				allQueueFlags |= prop->queueFlags;
			}
			B32 queuesSupported = (allQueueFlags & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT)) == (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT);
			if (!queuesSupported) {
				goto deviceUnsuitable;
			}
		}

		{ // Make sure all required extensions are supported
			uint32_t extensionCount = 0;
			VkExtensionProperties* availableExtensions;
			if (vkEnumerateDeviceExtensionProperties(*dev, nullptr, &extensionCount, nullptr) != VK_SUCCESS) {
				goto deviceUnsuitable;
			}
			availableExtensions = stackArena.alloc<VkExtensionProperties>(extensionCount);
			if (vkEnumerateDeviceExtensionProperties(*dev, nullptr, &extensionCount, availableExtensions) != VK_SUCCESS) {
				goto deviceUnsuitable;
			}
			for (U32 requiredExtensionIdx = 0; requiredExtensionIdx < ARRAY_COUNT(DEVICE_EXTENSIONS); requiredExtensionIdx++) {
				for (U32 presentExtensionIdx = 0; presentExtensionIdx < extensionCount; presentExtensionIdx++) {
					if (strcmp(DEVICE_EXTENSIONS[requiredExtensionIdx], availableExtensions[presentExtensionIdx].extensionName) == 0) {
						goto requiredExtensionPresent;
					}
				}
				goto deviceUnsuitable;
			requiredExtensionPresent:;
			}
		}

		{ // Make sure swapchain is supported
			VkSurfaceCapabilitiesKHR surfaceCapabilities;
			if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(*dev, desktopSwapchainData.surface, &surfaceCapabilities) != VK_SUCCESS) {
				goto deviceUnsuitable;
			}
			uint32_t formatCount;
			if (vkGetPhysicalDeviceSurfaceFormatsKHR(*dev, desktopSwapchainData.surface, &formatCount, nullptr) != VK_SUCCESS) {
				goto deviceUnsuitable;
			}
			uint32_t presentModeCount;
			if (vkGetPhysicalDeviceSurfacePresentModesKHR(*dev, desktopSwapchainData.surface, &presentModeCount, nullptr) != VK_SUCCESS) {
				goto deviceUnsuitable;
			}
			if (formatCount == 0 || presentModeCount == 0) {
				goto deviceUnsuitable;
			}
		}

		{ // Make sure we have all the features we need
			U32 requiredSubgroupOperations = VK_SUBGROUP_FEATURE_BASIC_BIT | VK_SUBGROUP_FEATURE_ARITHMETIC_BIT | VK_SUBGROUP_FEATURE_BALLOT_BIT | VK_SUBGROUP_FEATURE_VOTE_BIT;
			if (!(
				// All of the features we need to use go here
				deviceFeatures.samplerAnisotropy &&
				deviceFeatures.shaderSampledImageArrayDynamicIndexing &&
				deviceFeatures.multiDrawIndirect &&
				deviceFeatures.textureCompressionBC &&
				deviceFeatures.fullDrawIndexUint32 &&
				((subgroupProperties.supportedOperations & requiredSubgroupOperations) == requiredSubgroupOperations) &&
				deviceFeatures12.descriptorIndexing &&
				deviceFeatures12.vulkanMemoryModel &&
				deviceFeatures12.samplerFilterMinmax &&
				deviceFeatures12.shaderSampledImageArrayNonUniformIndexing &&
				deviceFeatures12.shaderStorageBufferArrayNonUniformIndexing &&
				deviceFeatures12.descriptorBindingSampledImageUpdateAfterBind &&
				deviceFeatures12.descriptorBindingUpdateUnusedWhilePending &&
				deviceFeatures12.descriptorBindingPartiallyBound &&
				deviceFeatures12.runtimeDescriptorArray &&
				deviceFeatures12.descriptorBindingVariableDescriptorCount &&
				deviceFeatures12.bufferDeviceAddress
				)) {
				goto deviceUnsuitable;
			}
		}

		{ // Device is valid, give it a score to see if we'll use it
			U32 currentPhysicalDeviceScore = 1;
			// Try to prefer discrete GPUs
			switch (deviceProperties.deviceType) {
			case VK_PHYSICAL_DEVICE_TYPE_CPU: currentPhysicalDeviceScore += 20000; break;
			case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: currentPhysicalDeviceScore += 40000; break;
			case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: currentPhysicalDeviceScore += 100000; break;
			case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: currentPhysicalDeviceScore += 90000; break;
			default: break;
			}
			currentPhysicalDeviceScore += deviceProperties.limits.maxImageDimension2D;
			if (currentPhysicalDeviceScore > bestPhysicalDeviceScore) {
				bestPhysicalDeviceScore = currentPhysicalDeviceScore;
				physicalDevice = *dev;
			}
		}
	deviceUnsuitable:;
		stackArena.stackPtr = oldStack;
	}
	if (physicalDevice == VK_NULL_HANDLE) {
		abort("Failed to find suitable vulkan device\n");
	}

	U32 familyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &familyCount, nullptr);
	VkQueueFamilyProperties* queueFamilyProperties = stackArena.alloc<VkQueueFamilyProperties>(familyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &familyCount, queueFamilyProperties);
	for (U32 i = 0; i < familyCount; i++) {
		U32 queueFlags = queueFamilyProperties[i].queueFlags;
		if (queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			graphicsFamily = i;
			familyPresentBits |= FAMILY_PRESENT_BIT_GRAPHICS;
		}
		if (queueFlags & VK_QUEUE_TRANSFER_BIT) {
			transferFamily = i;
			familyPresentBits |= FAMILY_PRESENT_BIT_TRANSFER;
		}
		if (queueFlags & VK_QUEUE_COMPUTE_BIT) {
			computeFamily = i;
			familyPresentBits |= FAMILY_PRESENT_BIT_COMPUTE;
		}
		if (familyPresentBits == (FAMILY_PRESENT_BIT_GRAPHICS | FAMILY_PRESENT_BIT_TRANSFER | FAMILY_PRESENT_BIT_COMPUTE)) {
			goto allNecessaryQueuesPresent;
		}
	}
	abort("Physical device did not have a graphics, transfer, and compute capable queue!");
allNecessaryQueuesPresent:;

	vkGetPhysicalDeviceProperties(physicalDevice, &physicalDeviceProperties);
	print("Using GPU: ");
	print(physicalDeviceProperties.deviceName);
	println();

	F32 queuePriority = 1.0F;
	VkDeviceQueueCreateInfo queueCreateInfos[3]{};
	U32 queueCreateInfoCount = 1;
	queueCreateInfos[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queueCreateInfos[0].flags = 0;
	queueCreateInfos[0].queueFamilyIndex = graphicsFamily;
	queueCreateInfos[0].queueCount = 1;
	queueCreateInfos[0].pQueuePriorities = &queuePriority;
	if (transferFamily != graphicsFamily) {
		queueCreateInfoCount++;
		queueCreateInfos[1].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfos[1].flags = 0;
		queueCreateInfos[1].queueFamilyIndex = transferFamily;
		queueCreateInfos[1].queueCount = 1;
		queueCreateInfos[1].pQueuePriorities = &queuePriority;
	}
	if (computeFamily != graphicsFamily && computeFamily != transferFamily) {
		queueCreateInfoCount++;
		queueCreateInfos[2].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfos[2].flags = 0;
		queueCreateInfos[2].queueFamilyIndex = computeFamily;
		queueCreateInfos[2].queueCount = 1;
		queueCreateInfos[2].pQueuePriorities = &queuePriority;
	}

	VkDeviceCreateInfo deviceCreateInfo{ VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
	deviceCreateInfo.queueCreateInfoCount = queueCreateInfoCount;
	deviceCreateInfo.pQueueCreateInfos = queueCreateInfos;
	deviceCreateInfo.enabledLayerCount = enabledLayers.size;
	deviceCreateInfo.ppEnabledLayerNames = enabledLayers.data;
	deviceCreateInfo.enabledExtensionCount = ARRAY_COUNT(DEVICE_EXTENSIONS);
	deviceCreateInfo.ppEnabledExtensionNames = DEVICE_EXTENSIONS;

	VkPhysicalDeviceFeatures deviceEnabledFeatures{};
	deviceEnabledFeatures.samplerAnisotropy = VK_TRUE;
	deviceEnabledFeatures.multiDrawIndirect = VK_TRUE;
	deviceEnabledFeatures.shaderSampledImageArrayDynamicIndexing = VK_TRUE;
	deviceEnabledFeatures.textureCompressionBC = VK_TRUE;
	deviceEnabledFeatures.fullDrawIndexUint32 = VK_TRUE;
	deviceCreateInfo.pEnabledFeatures = &deviceEnabledFeatures;
	VkPhysicalDeviceVulkan12Features deviceEnabledFeatures12{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
	deviceEnabledFeatures12.descriptorIndexing = VK_TRUE;
	deviceEnabledFeatures12.samplerFilterMinmax = VK_TRUE;
	deviceEnabledFeatures12.vulkanMemoryModel = VK_TRUE;
	deviceEnabledFeatures12.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
	deviceEnabledFeatures12.shaderStorageBufferArrayNonUniformIndexing = VK_TRUE;
	deviceEnabledFeatures12.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
	deviceEnabledFeatures12.descriptorBindingUpdateUnusedWhilePending = VK_TRUE;
	deviceEnabledFeatures12.descriptorBindingPartiallyBound = VK_TRUE;
	deviceEnabledFeatures12.descriptorBindingVariableDescriptorCount = VK_TRUE;
	deviceEnabledFeatures12.runtimeDescriptorArray = VK_TRUE;
	deviceEnabledFeatures12.bufferDeviceAddress = VK_TRUE;
	deviceCreateInfo.pNext = &deviceEnabledFeatures12;

	CHK_VK(vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &logicalDevice));

	load_device_vulkan_functions();

	vkGetDeviceQueue(logicalDevice, graphicsFamily, 0, &graphicsQueue);
	vkGetDeviceQueue(logicalDevice, transferFamily, 0, &transferQueue);
	vkGetDeviceQueue(logicalDevice, computeFamily, 0, &computeQueue);

	VkCommandPoolCreateInfo poolCreateInfo{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
	poolCreateInfo.flags = 0;
	poolCreateInfo.queueFamilyIndex = graphicsFamily;
	CHK_VK(vkCreateCommandPool(logicalDevice, &poolCreateInfo, VK_NULL_HANDLE, &graphicsCommandPools[0]));
	CHK_VK(vkCreateCommandPool(logicalDevice, &poolCreateInfo, VK_NULL_HANDLE, &graphicsCommandPools[1]));
	VkCommandBufferAllocateInfo cmdBufInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
	cmdBufInfo.commandPool = graphicsCommandPools[0];
	cmdBufInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cmdBufInfo.commandBufferCount = 1;
	CHK_VK(vkAllocateCommandBuffers(logicalDevice, &cmdBufInfo, &graphicsCommandBuffer));
	cmdBufInfo.commandPool = graphicsCommandPools[1];
	CHK_VK(vkAllocateCommandBuffers(logicalDevice, &cmdBufInfo, &inFlightGraphicsCommandBuffer));

	// Selecting memory types like this isn't actually a very good approach (but I can't be bothered to change it right now).
	// I should be selecting these dynamically based on the resource that needs to be allocated.
	hostMemoryTypeIndex = U32_MAX;
	deviceMemoryTypeIndex = U32_MAX;
	U32 hostHeapIdx{};
	U32 deviceHeapIdx{};
	VkPhysicalDeviceMemoryProperties memoryProperties;
	vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);
	for (U32 i = 0; i < memoryProperties.memoryTypeCount; i++) {
		VkMemoryType type = memoryProperties.memoryTypes[i];
		if (type.propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT && (hostMemoryTypeIndex == U32_MAX || memoryProperties.memoryHeaps[type.heapIndex].size > memoryProperties.memoryHeaps[hostHeapIdx].size)) {
			hostHeapIdx = type.heapIndex;
			hostMemoryFlags = type.propertyFlags;
			hostMemoryTypeIndex = i;
		}
		if (type.propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT && (deviceMemoryTypeIndex == U32_MAX || memoryProperties.memoryHeaps[type.heapIndex].size > memoryProperties.memoryHeaps[deviceHeapIdx].size)) {
			deviceHeapIdx = type.heapIndex;
			deviceMemoryFlags = type.propertyFlags;
			deviceMemoryTypeIndex = i;
		}
	}

	if (deviceMemoryTypeIndex == U32_MAX) {
		deviceMemoryTypeIndex = hostMemoryTypeIndex;
		deviceMemoryFlags = hostMemoryFlags;
	}
	if (hostMemoryTypeIndex == U32_MAX) {
		abort("Did not have required vulkan memory types\n");
	}

	graphicsStager.init(graphicsQueue, graphicsFamily);

	for (U32 i = 0; i < FRAMES_IN_FLIGHT; i++) {
		VkSemaphoreCreateInfo semaphoreInfo{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
		semaphoreInfo.flags = 0;
		CHK_VK(vkCreateSemaphore(logicalDevice, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]));
		VkFenceCreateInfo fenceInfo{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
		fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
		CHK_VK(vkCreateFence(logicalDevice, &fenceInfo, nullptr, &renderFinishedFences[i]));
	}

	desktopSwapchainData.create(Win32::framebufferWidth, Win32::framebufferHeight, VK_NULL_HANDLE);

	create_render_targets();

	DynamicVertexBuffer::init();
}

// We're going all in on builders this time. They're a really easy way to provide sensible defaults in a lightweight way

Framebuffer& Framebuffer::set_default() {
	renderPass = VK_NULL_HANDLE;
	framebufferWidth = 0;
	framebufferHeight = 0;
	attachmentCount = 0;
	return *this;
}

Framebuffer& Framebuffer::render_pass(VkRenderPass pass) {
	renderPass = pass;
	return *this;
}

Framebuffer& Framebuffer::dimensions(U32 width, U32 height) {
	framebufferWidth = width;
	framebufferHeight = height;
	return *this;
}

Framebuffer& Framebuffer::new_attachment(VkFormat imageFormat, VkImageUsageFlags usage, VkImageAspectFlags aspectMask) {
	if (attachmentCount == MAX_FRAMEBUFFER_ATTACHMENTS) {
		abort("Max framebuffer attachments exceeded");
	}

	FramebufferAttachment& attachment = attachments[attachmentCount++];
	attachment.imageFormat = imageFormat;
	attachment.imageUsage = usage;
	attachment.imageAspectMask = aspectMask;
	attachment.ownsImage = true;
	return *this;
}

Framebuffer& Framebuffer::existing_attachment(VkImageView view) {
	if (attachmentCount == MAX_FRAMEBUFFER_ATTACHMENTS) {
		abort("Max framebuffer attachments exceeded");
	}

	FramebufferAttachment& attachment = attachments[attachmentCount++];
	attachment.memory = VK_NULL_HANDLE;
	attachment.image = VK_NULL_HANDLE;
	attachment.imageView = view;
	attachment.ownsImage = false;
	return *this;
}

Framebuffer& Framebuffer::build() {
	if (renderPass == VK_NULL_HANDLE) {
		abort("Render pass must be set to create framebuffer");
	}
	if (framebufferWidth == 0 || framebufferHeight == 0) {
		abort("Framebuffer dimensions must be non-zero");
	}
	if (framebuffer != VK_NULL_HANDLE) {
		abort("Framebuffer already built");
	}

	VkImageView attachmentViews[MAX_FRAMEBUFFER_ATTACHMENTS];
	for (U32 i = 0; i < attachmentCount; i++) {
		FramebufferAttachment& attachment = attachments[i];

		if (attachment.ownsImage) {
			VkImageCreateInfo imageInfo{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
			imageInfo.flags = 0;
			imageInfo.imageType = VK_IMAGE_TYPE_2D;
			imageInfo.format = attachment.imageFormat;
			imageInfo.extent.width = framebufferWidth;
			imageInfo.extent.height = framebufferHeight;
			imageInfo.extent.depth = 1;
			imageInfo.mipLevels = 1;
			imageInfo.arrayLayers = 1;
			imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
			imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
			imageInfo.usage = attachment.imageUsage;
			imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			imageInfo.queueFamilyIndexCount = 0;
			imageInfo.pQueueFamilyIndices = nullptr;
			imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			VkImage image;
			CHK_VK(vkCreateImage(logicalDevice, &imageInfo, nullptr, &image));
			VkMemoryRequirements memoryRequirements;
			vkGetImageMemoryRequirements(logicalDevice, image, &memoryRequirements);
			if (!(memoryRequirements.memoryTypeBits & (1 << deviceMemoryTypeIndex))) {
				abort("Could not create framebuffer image in device local memory");
			}
			VkDeviceMemory memory;
			VkMemoryAllocateInfo allocInfo{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
			allocInfo.allocationSize = memoryRequirements.size;
			allocInfo.memoryTypeIndex = deviceMemoryTypeIndex;
			VkMemoryDedicatedAllocateInfo dedicatedAllocInfo{ VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO };
			dedicatedAllocInfo.image = image;
			allocInfo.pNext = &dedicatedAllocInfo;
			CHK_VK(vkAllocateMemory(logicalDevice, &allocInfo, nullptr, &memory));
			CHK_VK(vkBindImageMemory(logicalDevice, image, memory, 0));

			VkImageViewCreateInfo imageViewInfo{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
			imageViewInfo.flags = 0;
			imageViewInfo.image = image;
			imageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			imageViewInfo.format = attachment.imageFormat;
			imageViewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
			imageViewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
			imageViewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
			imageViewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
			imageViewInfo.subresourceRange.aspectMask = attachment.imageAspectMask;
			imageViewInfo.subresourceRange.baseMipLevel = 0;
			imageViewInfo.subresourceRange.levelCount = 1;
			imageViewInfo.subresourceRange.baseArrayLayer = 0;
			imageViewInfo.subresourceRange.layerCount = 1;
			VkImageView view;
			CHK_VK(vkCreateImageView(logicalDevice, &imageViewInfo, nullptr, &view));

			attachment.memory = memory;
			attachment.image = image;
			attachment.imageView = view;
		}
		
		attachmentViews[i] = attachment.imageView;
	}
	VkFramebufferCreateInfo framebufferInfo{ VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
	framebufferInfo.flags = 0;
	framebufferInfo.renderPass = renderPass;
	framebufferInfo.attachmentCount = attachmentCount;
	framebufferInfo.pAttachments = attachmentViews;
	framebufferInfo.width = framebufferWidth;
	framebufferInfo.height = framebufferHeight;
	framebufferInfo.layers = 1;
	CHK_VK(vkCreateFramebuffer(logicalDevice, &framebufferInfo, nullptr, &framebuffer));
	if (!addedToFreeList) {
		addedToFreeList = true;
		destroyLists.framebuffers.push_back(this);
	}
	return *this;;
}

void Framebuffer::destroy() {
	if (framebuffer != VK_NULL_HANDLE) {
		vkDestroyFramebuffer(logicalDevice, framebuffer, nullptr);
		framebuffer = VK_NULL_HANDLE;
		for (U32 i = 0; i < attachmentCount; i++) {
			FramebufferAttachment& attachment = attachments[i];
			if (attachment.ownsImage) {
				vkDestroyImageView(logicalDevice, attachment.imageView, nullptr);
			}
			if (attachment.image != VK_NULL_HANDLE) {
				vkDestroyImage(logicalDevice, attachment.image, nullptr);
			}
			if (attachment.memory != VK_NULL_HANDLE) {
				vkFreeMemory(logicalDevice, attachment.memory, nullptr);
			}
		}
	}
}

struct RenderPassBuilder {
	static constexpr U32 MAX_COLOR_ATTACHMENTS = 4;
	static constexpr U32 MAX_DEPTH_ATTACHMENTS = 1;
	static constexpr U32 MAX_ATTACHMENTS = MAX_COLOR_ATTACHMENTS + MAX_DEPTH_ATTACHMENTS;
	VkAttachmentDescription attachmentDescriptions[MAX_ATTACHMENTS];
	U32 numAttachments;
	B32 hasDepthAttachment;

	RenderPassBuilder& set_default() {
		numAttachments = 0;
		hasDepthAttachment = false;
		return *this;
	}

	RenderPassBuilder& color_attachment(VkFormat format) {
		if (hasDepthAttachment) {
			abort("Color attachments must be defined before depth attachment");
		}
		if (numAttachments == MAX_COLOR_ATTACHMENTS) {
			abort("Max attachment count exceeded");
		}
		VkAttachmentDescription& attachmentDesc = attachmentDescriptions[numAttachments++];
		attachmentDesc.flags = 0;
		attachmentDesc.format = format;
		attachmentDesc.samples = VK_SAMPLE_COUNT_1_BIT;
		attachmentDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachmentDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachmentDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachmentDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachmentDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachmentDesc.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		return *this;
	}

	RenderPassBuilder& depth_attachment(VkFormat format) {
		if (hasDepthAttachment) {
			abort("Color attachments must be defined before depth attachment");
		}
		if (numAttachments == MAX_COLOR_ATTACHMENTS) {
			abort("Max attachment count exceeded");
		}
		VkAttachmentDescription& attachmentDesc = attachmentDescriptions[numAttachments++];
		attachmentDesc.flags = 0;
		attachmentDesc.format = format;
		attachmentDesc.samples = VK_SAMPLE_COUNT_1_BIT;
		attachmentDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachmentDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachmentDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachmentDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachmentDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachmentDesc.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		hasDepthAttachment = true;
		return *this;
	}

	VkRenderPass build() {
		VkAttachmentReference colorRefs[MAX_COLOR_ATTACHMENTS];
		for (U32 i = 0; i < MAX_COLOR_ATTACHMENTS; i++) {
			colorRefs[i].attachment = i;
			colorRefs[i].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		}
		VkAttachmentReference depthRef;
		depthRef.attachment = numAttachments - 1;
		depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkSubpassDescription subpass{};
		subpass.flags = 0;
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.inputAttachmentCount = 0;
		subpass.pInputAttachments = nullptr;
		subpass.colorAttachmentCount = numAttachments - hasDepthAttachment;
		subpass.pColorAttachments = colorRefs;
		subpass.pResolveAttachments = nullptr;
		if (hasDepthAttachment) {
			subpass.pDepthStencilAttachment = &depthRef;
		} else {
			subpass.pDepthStencilAttachment = nullptr;
		}
		subpass.preserveAttachmentCount = 0;
		subpass.pPreserveAttachments = nullptr;
		VkRenderPassCreateInfo passInfo{ VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
		passInfo.flags = 0;
		passInfo.attachmentCount = numAttachments;
		passInfo.pAttachments = attachmentDescriptions;
		passInfo.subpassCount = 1;
		passInfo.pSubpasses = &subpass;
		passInfo.dependencyCount = 0;
		passInfo.pDependencies = nullptr;
		VkRenderPass renderPass;
		CHK_VK(vkCreateRenderPass(logicalDevice, &passInfo, nullptr, &renderPass));
		return renderPass;
	}
};

void create_render_targets() {
	mainRenderPass = RenderPassBuilder{}.set_default()
		.color_attachment(VK_FORMAT_R8G8B8A8_SRGB)
		.depth_attachment(VK_FORMAT_D32_SFLOAT)
		.build();
	mainFramebuffer.set_default()
		.render_pass(mainRenderPass)
		.dimensions(U32(Win32::framebufferWidth), U32(Win32::framebufferHeight))
		.new_attachment(VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, VK_IMAGE_ASPECT_COLOR_BIT)
		.new_attachment(VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_IMAGE_ASPECT_DEPTH_BIT)
		.build();
}

struct PipelineLayoutBuilder {
	static constexpr U32 MAX_PUSH_CONSTANTS = 1;
	static constexpr U32 MAX_SET_LAYOUTS = 4;
	VkPushConstantRange pushConstants[MAX_PUSH_CONSTANTS];
	VkDescriptorSetLayout setLayouts[MAX_SET_LAYOUTS];
	U32 pushConstantCount;
	U32 setLayoutCount;

	PipelineLayoutBuilder& set_default() {
		pushConstantCount = 0;
		return *this;
	}

	PipelineLayoutBuilder& push_constant(VkShaderStageFlags stage, U32 offset, U32 size) {
		if (pushConstantCount == MAX_PUSH_CONSTANTS) {
			// Since we create all pipeline layouts at startup, this will always trigger in testing, so it's almost like a static assert
			abort("PipelineLayoutInfo push constant capacity exceeded");
		}
		pushConstants[pushConstantCount++] = VkPushConstantRange{ stage, offset, size };
		return *this;
	}

	PipelineLayoutBuilder& set_layout(VkDescriptorSetLayout layout) {
		if (setLayoutCount == MAX_SET_LAYOUTS) {
			abort("PipelineLayoutInfo set layout capacity exceeded");
		}
		setLayouts[setLayoutCount++] = layout;
		return *this;
	}

	VkPipelineLayout build() {
		VkPipelineLayoutCreateInfo layoutInfo{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
		layoutInfo.flags = 0;
		layoutInfo.setLayoutCount = setLayoutCount;
		layoutInfo.pSetLayouts = setLayouts;
		layoutInfo.pushConstantRangeCount = pushConstantCount;
		layoutInfo.pPushConstantRanges = pushConstants;
		VkPipelineLayout layout;
		CHK_VK(vkCreatePipelineLayout(logicalDevice, &layoutInfo, VK_NULL_HANDLE, &layout));
		destroyLists.pipelineLayouts.push_back(layout);
		return layout;
	}
};




DescriptorSet& DescriptorSet::init() {
	setLayout = VK_NULL_HANDLE;
	descriptorSet = VK_NULL_HANDLE;
	descriptorPool = VK_NULL_HANDLE;
	bindingCount = 0;
	updateAfterBind = false;
	return *this;
}

DescriptorSet& DescriptorSet::basic_binding(U32 bindingIndex, VkShaderStageFlags stageFlags, VkDescriptorType descriptorType, U32 descriptorCount, VkDescriptorBindingFlags flags) {
	if (bindingCount == MAX_LAYOUT_BINDINGS) {
		abort("Ran out of layout bindings");
	}
	if (flags & VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT && bindingFlags[bindingCount - 1] & VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT) {
		abort("Variable descriptor count binding must be last");
	}
	if (flags & VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT) {
		updateAfterBind = true;
	}
	U32 bindingIdx = bindingCount++;
	VkDescriptorSetLayoutBinding& binding = bindings[bindingIdx];
	binding.binding = bindingIndex;
	binding.descriptorType = descriptorType;
	binding.descriptorCount = descriptorCount;
	binding.stageFlags = stageFlags;
	binding.pImmutableSamplers = nullptr;
	bindingFlags[bindingIdx] = flags;
	return *this;
}
DescriptorSet& DescriptorSet::storage_buffer(U32 bindingIndex, VkShaderStageFlags stageFlags) {
	return basic_binding(bindingIndex, stageFlags, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, 0);
}
DescriptorSet& DescriptorSet::uniform_buffer(U32 bindingIndex, VkShaderStageFlags stageFlags) {
	return basic_binding(bindingIndex, stageFlags, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, 0);
}
DescriptorSet& DescriptorSet::sampler(U32 bindingIndex, VkShaderStageFlags stageFlags) {
	return basic_binding(bindingIndex, stageFlags, VK_DESCRIPTOR_TYPE_SAMPLER, 1, 0);
}
DescriptorSet& DescriptorSet::texture_array(U32 bindingIndex, VkShaderStageFlags stageFlags, U32 maxArraySize, U32 arraySize) {
	variableDescriptorCountMax = maxArraySize;
	return basic_binding(bindingIndex, stageFlags, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, arraySize, VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT | VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT);
}

void DescriptorSet::build() {
	if (setLayout == VK_NULL_HANDLE) {
		VkDescriptorSetLayoutCreateInfo layoutInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
		layoutInfo.flags = 0;
		if (updateAfterBind) {
			layoutInfo.flags |= VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
		}
		if (bindingCount > 0 && bindingFlags[bindingCount - 1] & VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT) {
			swap(&bindings[bindingCount - 1].descriptorCount, &variableDescriptorCountMax);
		}
		layoutInfo.bindingCount = bindingCount;
		layoutInfo.pBindings = bindings;
		VkDescriptorSetLayoutBindingFlagsCreateInfo layoutInfoFlags{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO };
		layoutInfoFlags.bindingCount = bindingCount;
		layoutInfoFlags.pBindingFlags = bindingFlags;
		layoutInfo.pNext = &layoutInfoFlags;
		CHK_VK(vkCreateDescriptorSetLayout(logicalDevice, &layoutInfo, VK_NULL_HANDLE, &setLayout));

		if (bindingCount > 0 && bindingFlags[bindingCount - 1] & VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT) {
			swap(&bindings[bindingCount - 1].descriptorCount, &variableDescriptorCountMax);
		}
		destroyLists.descriptorSets.push_back(this);
	}

	VkDescriptorPoolSize requiredSizes[VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT + 1]{};
	for (U32 i = 0; i < bindingCount; i++) {
		requiredSizes[bindings[i].descriptorType].descriptorCount += bindings[i].descriptorCount;
		requiredSizes[bindings[i].descriptorType].type = bindings[i].descriptorType;
	}
	VkDescriptorPoolCreateInfo poolInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
	poolInfo.flags = 0;
	if (updateAfterBind) {
		poolInfo.flags |= VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
	}
	poolInfo.maxSets = 1;
	U32 poolSizeCount = 0;
	for (U32 i = 0; i < ARRAY_COUNT(requiredSizes); i++) {
		if (requiredSizes[i].descriptorCount > 0) {
			requiredSizes[poolSizeCount++] = requiredSizes[i];
		}
	}
	poolInfo.poolSizeCount = poolSizeCount;
	poolInfo.pPoolSizes = requiredSizes;
	CHK_VK(vkCreateDescriptorPool(logicalDevice, &poolInfo, nullptr, &descriptorPool));

	VkDescriptorSetAllocateInfo setAllocInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
	setAllocInfo.descriptorPool = descriptorPool;
	setAllocInfo.descriptorSetCount = 1;
	setAllocInfo.pSetLayouts = &setLayout;
	VkDescriptorSetVariableDescriptorCountAllocateInfo variableAllocInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO };
	if (bindingCount > 0 && bindingFlags[bindingCount - 1] & VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT) {
		variableAllocInfo.descriptorSetCount = 1;
		variableAllocInfo.pDescriptorCounts = &bindings[bindingCount - 1].descriptorCount;
		setAllocInfo.pNext = &variableAllocInfo;
	}
	CHK_VK(vkAllocateDescriptorSets(logicalDevice, &setAllocInfo, &descriptorSet));
}

// Should only be called before rendering anything in the current frame
void DescriptorSet::change_dynamic_array_length(U32 newDescriptorCount) {
	if (!(bindingFlags[bindingCount - 1] & VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT)) {
		abort("Attempted to change dynamic array length on a descriptor set without a variable descriptor count");
	}
	if (newDescriptorCount > variableDescriptorCountMax) {
		abort("New variable descriptor count was larger than maximum count");
	}
	frameDestroyLists[(currentFrameInFlight + 1) & 1].descriptorPools.push_back(descriptorPool);
	U32 oldDescriptorCount = bindings[bindingCount - 1].descriptorCount;
	bindings[bindingCount - 1].descriptorCount = newDescriptorCount;
	VkDescriptorSet oldDescriptorSet = descriptorSet;

	this->build();

	VkCopyDescriptorSet copies[MAX_LAYOUT_BINDINGS];
	for (U32 i = 0; i < bindingCount; i++) {
		VkCopyDescriptorSet& copy = copies[i];
		copy.sType = VK_STRUCTURE_TYPE_COPY_DESCRIPTOR_SET;
		copy.pNext = nullptr;
		copy.srcSet = oldDescriptorSet;
		copy.srcBinding = i;
		copy.srcArrayElement = 0;
		copy.dstSet = descriptorSet;
		copy.dstBinding = i;
		copy.dstArrayElement = 0;
		copy.descriptorCount = i == bindingCount - 1 ? min(oldDescriptorCount, newDescriptorCount) : bindings[i].descriptorCount;
	}
	vkUpdateDescriptorSets(logicalDevice, 0, nullptr, bindingCount, copies);
}

U32 DescriptorSet::current_dynamic_array_length() {
	if (!(bindingFlags[bindingCount - 1] & VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT)) {
		abort("Attempted to query dynamic array length on a descriptor set without a variable descriptor count");
	}
	return bindings[bindingCount - 1].descriptorCount;
}


struct GraphicsPipelineBuilder {
	static constexpr U32 MAX_ATTRIBUTE_DESCRIPTIONS = 8;
	static constexpr U32 MAX_BINDING_DESCRIPTIONS = 8;

	StrA vertexShaderFileName;
	StrA fragmentShaderFileName;
	VkPipelineLayout pipelineLayout;
	VkVertexInputAttributeDescription attributeDescriptions[MAX_ATTRIBUTE_DESCRIPTIONS];
	VkVertexInputBindingDescription bindingDescriptions[MAX_BINDING_DESCRIPTIONS];
	U32 attributeDescriptionCount;
	U32 bindingDescriptionCount;
	VkBool32 blendEnabled;
	VkBlendFactor srcBlendFactor;
	VkBlendFactor dstBlendFactor;

	GraphicsPipelineBuilder& set_default() {
		vertexShaderFileName = StrA{};
		fragmentShaderFileName = StrA{};
		pipelineLayout = VK_NULL_HANDLE;
		attributeDescriptionCount = 0;
		bindingDescriptionCount = 0;
		blendEnabled = VK_FALSE;
		srcBlendFactor = VK_BLEND_FACTOR_ONE;
		dstBlendFactor = VK_BLEND_FACTOR_ZERO;
		return *this;
	}
	GraphicsPipelineBuilder& vertex_shader(StrA shaderFile) {
		vertexShaderFileName = shaderFile;
		return *this;
	}
	GraphicsPipelineBuilder& fragment_shader(StrA shaderFile) {
		fragmentShaderFileName = shaderFile;
		return *this;
	}
	GraphicsPipelineBuilder& blending(VkBlendFactor srcFactor, VkBlendFactor dstFactor) {
		blendEnabled = VK_TRUE;
		srcBlendFactor = srcFactor;
		dstBlendFactor = dstFactor;
		return *this;
	}
	GraphicsPipelineBuilder& layout(VkPipelineLayout layout) {
		pipelineLayout = layout;
		return *this;
	}
	GraphicsPipelineBuilder& vertex_attribute(U32 binding, U32 location, VkFormat format, U32 size) {
		if (attributeDescriptionCount == MAX_ATTRIBUTE_DESCRIPTIONS) {
			abort("Maximum attribute descriptions exceeded");
		}
		if (binding != bindingDescriptionCount && binding != bindingDescriptionCount - 1) {
			abort("Binding must be either the same as current or the next binding");
		}
		if (binding == MAX_BINDING_DESCRIPTIONS) {
			abort("Maximum binding descriptions exceeded");
		}
		VkVertexInputAttributeDescription& attr = attributeDescriptions[attributeDescriptionCount++];
		VkVertexInputBindingDescription& bind = bindingDescriptions[binding];
		if (binding == bindingDescriptionCount) {
			bind.binding = binding;
			bind.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
			bind.stride = 0;
			bindingDescriptionCount++;
		}
		attr.location = location;
		attr.binding = binding;
		attr.format = format;
		attr.offset = bind.stride;
		bind.stride += size;
		return *this;
	}

	VkPipeline build() {
		if (pipelineLayout == VK_NULL_HANDLE) {
			abort("Must set pipeline layout to build VkPipeline");
		}
		if (vertexShaderFileName.is_empty() || fragmentShaderFileName.is_empty()) {
			abort("Must set shader file name to build VkPipeline");
		}

		MemoryArena& stackArena = get_scratch_arena();

		U64 stackArenaFrame0 = stackArena.stackPtr;
		U32 vertSpirvDwordCount;
		U32* vertSpirv = read_full_file_to_arena<U32>(&vertSpirvDwordCount, stackArena, vertexShaderFileName);
		if (vertSpirv == nullptr) {
			print("File read failed: ");
			println(vertexShaderFileName);
			abort("Failed reading vertex spv file");
		}
		U32 fragSpirvDwordCount;
		U32* fragSpirv = read_full_file_to_arena<U32>(&fragSpirvDwordCount, stackArena, fragmentShaderFileName);
		if (fragSpirv == nullptr) {
			print("File read failed: ");
			println(fragmentShaderFileName);
			abort("Failed reading fragment spv file");
		}
		VkShaderModuleCreateInfo moduleInfo{ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
		moduleInfo.flags = 0;
		moduleInfo.codeSize = vertSpirvDwordCount * sizeof(U32);
		moduleInfo.pCode = vertSpirv;
		VkShaderModule vertShaderModule;
		CHK_VK(vkCreateShaderModule(logicalDevice, &moduleInfo, VK_NULL_HANDLE, &vertShaderModule));
		moduleInfo.codeSize = fragSpirvDwordCount * sizeof(U32);
		moduleInfo.pCode = fragSpirv;
		VkShaderModule fragShaderModule;
		CHK_VK(vkCreateShaderModule(logicalDevice, &moduleInfo, VK_NULL_HANDLE, &fragShaderModule));
		stackArena.stackPtr = stackArenaFrame0;

		VkPipelineShaderStageCreateInfo stageInfos[]{ { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO }, { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO } };
		VkPipelineShaderStageCreateInfo& vertexStageInfo = stageInfos[0];
		vertexStageInfo.flags = 0;
		vertexStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
		vertexStageInfo.module = vertShaderModule;
		vertexStageInfo.pName = "main";
		vertexStageInfo.pSpecializationInfo = nullptr;
		VkPipelineShaderStageCreateInfo& fragmentStageInfo = stageInfos[1];
		fragmentStageInfo.flags = 0;
		fragmentStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		fragmentStageInfo.module = fragShaderModule;
		fragmentStageInfo.pName = "main";
		fragmentStageInfo.pSpecializationInfo = nullptr;

		VkPipelineVertexInputStateCreateInfo vertexInputStateInfo{ VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
		vertexInputStateInfo.flags = 0;
		vertexInputStateInfo.vertexBindingDescriptionCount = bindingDescriptionCount;
		vertexInputStateInfo.pVertexBindingDescriptions = bindingDescriptions;
		vertexInputStateInfo.vertexAttributeDescriptionCount = attributeDescriptionCount;
		vertexInputStateInfo.pVertexAttributeDescriptions = attributeDescriptions;

		VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateInfo{ VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
		inputAssemblyStateInfo.flags = 0;
		inputAssemblyStateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		inputAssemblyStateInfo.primitiveRestartEnable = VK_FALSE;

		// Viewport and scissor don't matter here since we set them as dynamic state
		VkViewport viewport{};
		viewport.x = 0.0F;
		viewport.y = 0.0F;
		viewport.width = F32(Win32::framebufferWidth);
		viewport.height = F32(Win32::framebufferHeight);
		viewport.minDepth = 0.0F;
		viewport.maxDepth = 1.0F;
		VkRect2D scissor{};
		scissor.offset.x = 0;
		scissor.offset.y = 0;
		scissor.extent.width = U32(Win32::framebufferWidth);
		scissor.extent.height = U32(Win32::framebufferHeight);
		VkPipelineViewportStateCreateInfo inputViewportStateInfo{ VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
		inputViewportStateInfo.flags = 0;
		inputViewportStateInfo.viewportCount = 1;
		inputViewportStateInfo.pViewports = &viewport;
		inputViewportStateInfo.scissorCount = 1;
		inputViewportStateInfo.pScissors = &scissor;

		VkPipelineRasterizationStateCreateInfo rasterizationStateInfo{ VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
		rasterizationStateInfo.flags = 0;
		rasterizationStateInfo.depthClampEnable = VK_FALSE;
		rasterizationStateInfo.rasterizerDiscardEnable = VK_FALSE;
		rasterizationStateInfo.polygonMode = VK_POLYGON_MODE_FILL;
		rasterizationStateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
		rasterizationStateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
		rasterizationStateInfo.depthBiasEnable = VK_FALSE;
		rasterizationStateInfo.depthBiasConstantFactor = 0.0F;
		rasterizationStateInfo.depthBiasClamp = 0.0F;
		rasterizationStateInfo.depthBiasSlopeFactor = 0.0F;
		rasterizationStateInfo.lineWidth = 1.0F;

		VkPipelineMultisampleStateCreateInfo multisampleStateInfo{ VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
		multisampleStateInfo.flags = 0;
		multisampleStateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		multisampleStateInfo.sampleShadingEnable = VK_FALSE;
		multisampleStateInfo.minSampleShading = 0.0F;
		multisampleStateInfo.pSampleMask = nullptr;
		multisampleStateInfo.alphaToCoverageEnable = VK_FALSE;
		multisampleStateInfo.alphaToOneEnable = VK_FALSE;

		VkPipelineDepthStencilStateCreateInfo depthStencilStateInfo{ VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
		depthStencilStateInfo.flags = 0;
		depthStencilStateInfo.depthTestEnable = VK_TRUE;
		depthStencilStateInfo.depthWriteEnable = VK_TRUE;
		depthStencilStateInfo.depthCompareOp = VK_COMPARE_OP_GREATER_OR_EQUAL;
		depthStencilStateInfo.depthBoundsTestEnable = VK_FALSE;
		depthStencilStateInfo.stencilTestEnable = VK_FALSE;
		depthStencilStateInfo.front = VkStencilOpState{};
		depthStencilStateInfo.back = VkStencilOpState{};
		depthStencilStateInfo.minDepthBounds = 0.0F;
		depthStencilStateInfo.maxDepthBounds = 1.0F;

		VkPipelineColorBlendAttachmentState mainAttachmentState{};
		mainAttachmentState.blendEnable = blendEnabled;
		mainAttachmentState.srcColorBlendFactor = srcBlendFactor;
		mainAttachmentState.dstColorBlendFactor = dstBlendFactor;
		mainAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
		mainAttachmentState.srcAlphaBlendFactor = srcBlendFactor;
		mainAttachmentState.dstAlphaBlendFactor = dstBlendFactor;
		mainAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;
		mainAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		VkPipelineColorBlendStateCreateInfo colorBlendStateInfo{ VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
		colorBlendStateInfo.flags = 0;
		colorBlendStateInfo.logicOpEnable = VK_FALSE;
		colorBlendStateInfo.logicOp = VK_LOGIC_OP_COPY;
		colorBlendStateInfo.attachmentCount = 1;
		colorBlendStateInfo.pAttachments = &mainAttachmentState;
		colorBlendStateInfo.blendConstants[0] = 0.0F;
		colorBlendStateInfo.blendConstants[1] = 0.0F;
		colorBlendStateInfo.blendConstants[2] = 0.0F;
		colorBlendStateInfo.blendConstants[3] = 0.0F;

		VkDynamicState dynamicStates[]{ VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
		VkPipelineDynamicStateCreateInfo dynamicStateInfo{ VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
		dynamicStateInfo.flags = 0;
		dynamicStateInfo.dynamicStateCount = ARRAY_COUNT(dynamicStates);
		dynamicStateInfo.pDynamicStates = dynamicStates;

		VkGraphicsPipelineCreateInfo pipelineInfo{ VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
		pipelineInfo.flags = 0;
		pipelineInfo.stageCount = 2;
		pipelineInfo.pStages = stageInfos;
		pipelineInfo.pVertexInputState = &vertexInputStateInfo;
		pipelineInfo.pInputAssemblyState = &inputAssemblyStateInfo;
		pipelineInfo.pTessellationState = nullptr;
		pipelineInfo.pViewportState = &inputViewportStateInfo;
		pipelineInfo.pRasterizationState = &rasterizationStateInfo;
		pipelineInfo.pMultisampleState = &multisampleStateInfo;
		pipelineInfo.pDepthStencilState = &depthStencilStateInfo;
		pipelineInfo.pColorBlendState = &colorBlendStateInfo;
		pipelineInfo.pDynamicState = &dynamicStateInfo;
		pipelineInfo.layout = pipelineLayout;
		pipelineInfo.renderPass = mainRenderPass;
		pipelineInfo.subpass = 0;
		pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
		pipelineInfo.basePipelineIndex = 0;
		VkPipeline pipeline;
		CHK_VK(vkCreateGraphicsPipelines(logicalDevice, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline));

		vkDestroyShaderModule(logicalDevice, vertShaderModule, nullptr);
		vkDestroyShaderModule(logicalDevice, fragShaderModule, nullptr);

		destroyLists.pipelines.push_back(pipeline);

		return pipeline;
	}
};

struct ComputePipelineBuilder {
	VkPipelineLayout pipelineLayout;
	StrA shaderFileName;

	ComputePipelineBuilder& set_default() {
		pipelineLayout = VK_NULL_HANDLE;
		shaderFileName = StrA{};
		return *this;
	}

	ComputePipelineBuilder& shader_name(StrA fileName) {
		shaderFileName = fileName;
		return *this;
	}

	ComputePipelineBuilder& layout(VkPipelineLayout layout) {
		pipelineLayout = layout;
		return *this;
	}

	VkPipeline build() {
		if (pipelineLayout == VK_NULL_HANDLE) {
			abort("Must set pipeline layout to build VkPipeline");
		}
		if (shaderFileName.is_empty()) {
			abort("Must set shader file name to build VkPipeline");
		}

		MemoryArena& stackArena = get_scratch_arena();

		U64 stackArenaFrame0 = stackArena.stackPtr;
		U32 spirvDwordCount;
		U32* spirv = read_full_file_to_arena<U32>(&spirvDwordCount, stackArena, shaderFileName);
		if (spirv == nullptr) {
			print("File read failed: ");
			println(shaderFileName);
			abort("Failed reading spv file");
		}
		VkShaderModuleCreateInfo moduleInfo{ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
		moduleInfo.flags = 0;
		moduleInfo.codeSize = spirvDwordCount * sizeof(U32);
		moduleInfo.pCode = spirv;
		VkShaderModule shaderModule;
		CHK_VK(vkCreateShaderModule(logicalDevice, &moduleInfo, nullptr, &shaderModule));
		stackArena.stackPtr = stackArenaFrame0;

		VkComputePipelineCreateInfo pipelineInfo{ VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
		pipelineInfo.flags = 0;
		pipelineInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		pipelineInfo.stage.flags = 0;
		pipelineInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
		pipelineInfo.stage.module = shaderModule;
		pipelineInfo.stage.pName = "compute_main";
		pipelineInfo.stage.pSpecializationInfo = nullptr;
		pipelineInfo.layout = pipelineLayout;
		pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
		pipelineInfo.basePipelineIndex = -1;
		VkPipeline pipeline;
		CHK_VK(vkCreateComputePipelines(logicalDevice, VK_NULL_HANDLE, 1, &pipelineInfo, VK_NULL_HANDLE, &pipeline));

		vkDestroyShaderModule(logicalDevice, shaderModule, nullptr);

		destroyLists.pipelines.push_back(pipeline);

		return pipeline;
	}
};

VkSampler make_sampler(VkFilter filter, VkSamplerMipmapMode mipmapMode, VkSamplerAddressMode addressMode, F32 anisotropy) {
	VkSamplerCreateInfo samplerInfo{ VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
	samplerInfo.flags = 0;
	samplerInfo.magFilter = filter;
	samplerInfo.minFilter = filter;
	samplerInfo.mipmapMode = mipmapMode;
	samplerInfo.addressModeU = samplerInfo.addressModeV = samplerInfo.addressModeW = addressMode;
	samplerInfo.mipLodBias = 0.0F;
	anisotropy = clamp(anisotropy, 1.0F, physicalDeviceProperties.limits.maxSamplerAnisotropy);
	samplerInfo.anisotropyEnable = anisotropy == 1.0F ? VK_FALSE : VK_TRUE;
	samplerInfo.maxAnisotropy = anisotropy;
	samplerInfo.compareEnable = VK_FALSE;
	samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
	samplerInfo.minLod = 0.0F;
	samplerInfo.maxLod = VK_LOD_CLAMP_NONE;
	samplerInfo.borderColor = VkBorderColor{};
	samplerInfo.unnormalizedCoordinates = VK_FALSE;
	VkSampler sampler;
	CHK_VK(vkCreateSampler(logicalDevice, &samplerInfo, nullptr, &sampler));
	destroyLists.samplers.push_back(sampler);
	return sampler;
}

#pragma pack(push, 1)
struct UIPipelineRenderData {
	V2F32 screenDimensions;
	U64 vertexBufferPointer;
	U64 clipBoxesPointer;
};
struct UIVertex {
	V3F32 pos;
	V2F32 tex;
	U32 color;
	U32 texIdx;
	U32 flags;
};
#pragma pack(pop)

void load_pipelines_and_descriptors() {
	drawDataDescriptorSet.init()
		.sampler(0, VK_SHADER_STAGE_FRAGMENT_BIT)
		.texture_array(1, VK_SHADER_STAGE_FRAGMENT_BIT, Textures::MAX_TEXTURE_COUNT, Textures::currentTextureMaxCount)
		.build();

	linearSampler = make_sampler(VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT, 4.0F);

	VkWriteDescriptorSet writeDesc{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
	writeDesc.dstSet = drawDataDescriptorSet.descriptorSet;
	writeDesc.dstBinding = 0;
	writeDesc.dstArrayElement = 0;
	writeDesc.descriptorCount = 1;
	writeDesc.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
	VkDescriptorImageInfo imageInfo{};
	imageInfo.sampler = linearSampler;
	writeDesc.pImageInfo = &imageInfo;
	vkUpdateDescriptorSets(logicalDevice, 1, &writeDesc, 0, nullptr);

	// Pipelines
	uiPipelineLayout = PipelineLayoutBuilder{}.set_default()
		.push_constant(VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(UIPipelineRenderData))
		.set_layout(drawDataDescriptorSet.setLayout)
		.build();
	uiPipeline = GraphicsPipelineBuilder{}.set_default()
		.layout(uiPipelineLayout)
		.vertex_shader("./resources/shaders/spirv/ui.vspv"sa)
		.fragment_shader("./resources/shaders/spirv/ui.fspv"sa)
		.blending(VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA)
		.build();
}

void finish_startup() {
	graphicsStager.flush();
	CHK_VK(vkDeviceWaitIdle(logicalDevice));
}

enum FrameBeginResult {
	FRAME_BEGIN_RESULT_CONTINUE,
	FRAME_BEGIN_RESULT_TRY_AGAIN,
	FRAME_BEGIN_RESULT_DONT_RENDER
};

void recreate_swapchain() {
	desktopSwapchainData.resize(Win32::framebufferWidth, Win32::framebufferHeight);
	mainFramebuffer.destroy();
	if (desktopSwapchainData.swapchain) {
		mainFramebuffer.dimensions(desktopSwapchainData.width, desktopSwapchainData.height).build();
	}
	Win32::shouldRecreateSwapchain = false;
}

FrameBeginResult begin_frame() {
	if (Win32::shouldRecreateSwapchain) {
		recreate_swapchain();
	}
	if (desktopSwapchainData.swapchain == VK_NULL_HANDLE) {
		return FRAME_BEGIN_RESULT_DONT_RENDER;
	}

	CHK_VK(vkWaitForFences(logicalDevice, 1, &renderFinishedFences[currentFrameInFlight], VK_TRUE, U64_MAX));
	CHK_VK(vkResetFences(logicalDevice, 1, &renderFinishedFences[currentFrameInFlight]));

	for (VkDescriptorPool pool : frameDestroyLists[currentFrameInFlight].descriptorPools) {
		vkDestroyDescriptorPool(logicalDevice, pool, nullptr);
	}
	frameDestroyLists[currentFrameInFlight].descriptorPools.clear();

	VkResult acquireResult;
	do {
		acquireResult = vkAcquireNextImageKHR(logicalDevice, desktopSwapchainData.swapchain, U64_MAX, desktopSwapchainData.swapchainAcquireSemaphores[currentFrameInFlight], VK_NULL_HANDLE, &desktopSwapchainData.swapchainImageIdx);
		if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR || acquireResult == VK_SUBOPTIMAL_KHR) {
			recreate_swapchain();
		} else {
			CHK_VK(acquireResult);
		}
	} while (acquireResult != VK_SUCCESS);

	CHK_VK(vkResetCommandPool(logicalDevice, graphicsCommandPools[0], 0));

	VkCommandBufferBeginInfo cmdBufInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	cmdBufInfo.flags = 0;
	cmdBufInfo.pInheritanceInfo = nullptr;
	CHK_VK(vkBeginCommandBuffer(graphicsCommandBuffer, &cmdBufInfo));
	return FRAME_BEGIN_RESULT_CONTINUE;
}

void end_frame() {
	// Framebuffer image -> transfer src optimal
	VkImageMemoryBarrier imageTransferBarrier{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
	imageTransferBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	imageTransferBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
	imageTransferBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	imageTransferBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	imageTransferBarrier.srcQueueFamilyIndex = graphicsFamily;
	imageTransferBarrier.dstQueueFamilyIndex = graphicsFamily;
	imageTransferBarrier.image = mainFramebuffer.attachments[0].image;
	imageTransferBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	imageTransferBarrier.subresourceRange.baseMipLevel = 0;
	imageTransferBarrier.subresourceRange.levelCount = 1;
	imageTransferBarrier.subresourceRange.baseArrayLayer = 0;
	imageTransferBarrier.subresourceRange.layerCount = 1;
	vkCmdPipelineBarrier(graphicsCommandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0, nullptr, 0, nullptr, 1, &imageTransferBarrier);
	// Swapchain image -> transfer dst optimal
	imageTransferBarrier.srcAccessMask = VK_ACCESS_NONE_KHR;
	imageTransferBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	imageTransferBarrier.image = desktopSwapchainData.swapchainImages[desktopSwapchainData.swapchainImageIdx];
	imageTransferBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageTransferBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	vkCmdPipelineBarrier(graphicsCommandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageTransferBarrier);

	// Copy main framebuffer to swapchain image
	VkImageBlit finalBlit{};
	finalBlit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	finalBlit.srcSubresource.mipLevel = 0;
	finalBlit.srcSubresource.baseArrayLayer = 0;
	finalBlit.srcSubresource.layerCount = 1;
	finalBlit.srcOffsets[0] = VkOffset3D{ 0, 0, 0 };
	finalBlit.srcOffsets[1] = VkOffset3D{ I32(mainFramebuffer.framebufferWidth), I32(mainFramebuffer.framebufferHeight), 1 };
	finalBlit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	finalBlit.dstSubresource.mipLevel = 0;
	finalBlit.dstSubresource.baseArrayLayer = 0;
	finalBlit.dstSubresource.layerCount = 1;
	finalBlit.dstOffsets[0] = VkOffset3D{ 0, 0, 0 };
	finalBlit.dstOffsets[1] = VkOffset3D{ I32(desktopSwapchainData.width), I32(desktopSwapchainData.height), 1 };
	vkCmdBlitImage(graphicsCommandBuffer, mainFramebuffer.attachments[0].image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, desktopSwapchainData.swapchainImages[desktopSwapchainData.swapchainImageIdx], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &finalBlit, VK_FILTER_NEAREST);

	// swapchain image -> present src optimal
	imageTransferBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	imageTransferBarrier.dstAccessMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
	imageTransferBarrier.image = desktopSwapchainData.swapchainImages[desktopSwapchainData.swapchainImageIdx];
	imageTransferBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	imageTransferBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	vkCmdPipelineBarrier(graphicsCommandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageTransferBarrier);

	CHK_VK(vkEndCommandBuffer(graphicsCommandBuffer));

	VkSubmitInfo submitInfo{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = &desktopSwapchainData.swapchainAcquireSemaphores[currentFrameInFlight];
	VkPipelineStageFlags waitStages[]{ VK_PIPELINE_STAGE_TRANSFER_BIT };
	submitInfo.pWaitDstStageMask = waitStages;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &graphicsCommandBuffer;
	VkSemaphore signalSemaphores[]{ renderFinishedSemaphores[currentFrameInFlight] };
	submitInfo.signalSemaphoreCount = ARRAY_COUNT(signalSemaphores);
	submitInfo.pSignalSemaphores = signalSemaphores;
	CHK_VK(vkQueueSubmit(graphicsQueue, 1, &submitInfo, renderFinishedFences[currentFrameInFlight]));
	

	VkResult imagePresentResult = VK_SUCCESS;
	VkPresentInfoKHR presentInfo{ VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &renderFinishedSemaphores[currentFrameInFlight];
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &desktopSwapchainData.swapchain;
	presentInfo.pImageIndices = &desktopSwapchainData.swapchainImageIdx;
	presentInfo.pResults = &imagePresentResult;
	VkResult queuePresentResult;
	//LOG_TIME("Present: ")
	queuePresentResult = vkQueuePresentKHR(graphicsQueue, &presentInfo);
	if (queuePresentResult == VK_ERROR_OUT_OF_DATE_KHR || queuePresentResult == VK_SUBOPTIMAL_KHR) {
		recreate_swapchain();
	} else {
		CHK_VK(queuePresentResult);
	}

	currentFrameInFlight = (currentFrameInFlight + 1) & 1;
	swap(&graphicsCommandPools[0], &graphicsCommandPools[1]);
	swap(&graphicsCommandBuffer, &inFlightGraphicsCommandBuffer);
	swap(&frameArena, &lastFrameArena);
	frameArena.reset();
}

void end_vulkan() {
	Textures::destroy_all();
	DynamicVertexBuffer::destroy();
	desktopSwapchainData.destroy();
	graphicsStager.destroy();
	for (U32 i = 0; i < FRAMES_IN_FLIGHT; i++) {
		vkDestroySemaphore(logicalDevice, renderFinishedSemaphores[i], nullptr);
		vkDestroyFence(logicalDevice, renderFinishedFences[i], nullptr);
	}
	for (Framebuffer* framebuffer : destroyLists.framebuffers) {
		framebuffer->destroy();
	}
	for (DescriptorSet* set : destroyLists.descriptorSets) {
		vkDestroyDescriptorPool(logicalDevice, set->descriptorPool, nullptr);
		vkDestroyDescriptorSetLayout(logicalDevice, set->setLayout, nullptr);
	}
	for (VkSampler sampler : destroyLists.samplers) {
		vkDestroySampler(logicalDevice, sampler, nullptr);
	}
	for (VkPipeline pipeline : destroyLists.pipelines) {
		vkDestroyPipeline(logicalDevice, pipeline, nullptr);
	}
	for (VkPipelineLayout layout : destroyLists.pipelineLayouts) {
		vkDestroyPipelineLayout(logicalDevice, layout, nullptr);
	}
	if (graphicsCommandPools[0] != VK_NULL_HANDLE) {
		vkDestroyCommandPool(logicalDevice, graphicsCommandPools[0], nullptr);
		vkDestroyCommandPool(logicalDevice, graphicsCommandPools[1], nullptr);
	}
	if (mainRenderPass != VK_NULL_HANDLE) {
		vkDestroyRenderPass(logicalDevice, mainRenderPass, nullptr);
	}
	vkDestroyDevice(logicalDevice, nullptr);
#if VK_DEBUG != 0
	vkDestroyDebugUtilsMessengerEXT(vkInstance, messenger, nullptr);
#endif
	vkDestroyInstance(vkInstance, nullptr);
}

}