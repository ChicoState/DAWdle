#pragma once

#include "DrillLib.h"
#include "VK_decl.h"
#include "VKStaging_decl.h"
#include "PNG.h"
#include "MSDFGenerator.h"
#include "Textures_decl.h"

namespace Textures {

enum TextureFormat : U32 {
	TEXTURE_FORMAT_NULL,
	TEXTURE_FORMAT_RGBA_U8,
	TEXTURE_FORMAT_RGBA_U16,
	TEXTURE_FORMAT_RGBA_BC7,
	TEXTURE_FORMAT_COUNT
};
// I forgot what this enum was for. Perhaps to control mipmap generation? SRGB vs linear? I'm sure I needed it for something
enum TextureType : U32 {
	TEXTURE_TYPE_COLOR,
	TEXTURE_TYPE_NORMAL_MAP,
	TEXTURE_TYPE_MSDF
};
struct Texture {
	VkImage image;
	VkImageView imageView;
	U32 index;
	TextureType type;
};

Texture missing;
Texture simpleWhite;
Texture fontAtlas;
Texture dropdownUp;
Texture dropdownDown;
Texture uiX;
Texture nodeConnect;


ArenaArrayList<Texture*> allTextures;
ArenaArrayList<VkDeviceMemory> memoryUsed;
const U64 blockAllocationSize = 32 * MEGABYTE;
U64 currentMemoryBlockOffset = 0;

void alloc_texture_memory(VkDeviceMemory* memoryOut, VkDeviceSize* allocatedOffsetOut, VkMemoryRequirements memoryRequirements) {
	if (!(memoryRequirements.memoryTypeBits & (1 << VK::deviceMemoryTypeIndex))) {
		abort("Texture can't be in device memory? Shouldn't happen.");
	}
	if (memoryUsed.empty() || I64(blockAllocationSize - ALIGN_HIGH(currentMemoryBlockOffset, memoryRequirements.alignment)) < I64(memoryRequirements.size)) {
		VkMemoryAllocateInfo allocInfo{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
		allocInfo.allocationSize = max(blockAllocationSize, memoryRequirements.size);
		allocInfo.memoryTypeIndex = VK::deviceMemoryTypeIndex;
		VkDeviceMemory memory;
		CHK_VK(VK::vkAllocateMemory(VK::logicalDevice, &allocInfo, nullptr, &memory));
		memoryUsed.push_back(memory);
		currentMemoryBlockOffset = 0;
	}
	U64 allocatedOffset = ALIGN_HIGH(currentMemoryBlockOffset, memoryRequirements.alignment);
	currentMemoryBlockOffset = allocatedOffset + memoryRequirements.size;

	*memoryOut = memoryUsed.back();
	*allocatedOffsetOut = allocatedOffset;
}

void create_texture(Texture* result, void* data, U32 width, U32 height, TextureFormat format, TextureType type) {
	Texture& tex = *result;
	tex.type = type;
	VkImageCreateInfo imageCreateInfo{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
	imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
	imageCreateInfo.format = type == TEXTURE_TYPE_COLOR ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM;
	imageCreateInfo.extent = VkExtent3D{ width, height, 1 };
	imageCreateInfo.mipLevels = 1;
	imageCreateInfo.arrayLayers = 1;
	imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageCreateInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageCreateInfo.queueFamilyIndexCount = 0;
	imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	CHK_VK(VK::vkCreateImage(VK::logicalDevice, &imageCreateInfo, nullptr, &tex.image));

	VkMemoryRequirements memoryRequirements;
	VK::vkGetImageMemoryRequirements(VK::logicalDevice, tex.image, &memoryRequirements);

	VkDeviceMemory linearBlock;
	VkDeviceSize memoryOffset;
	alloc_texture_memory(&linearBlock, &memoryOffset, memoryRequirements);
	CHK_VK(VK::vkBindImageMemory(VK::logicalDevice, tex.image, linearBlock, memoryOffset));

	VkCommandBuffer stagingCmdBuf = VK::graphicsStager.prepare_for_image_upload(width, height, sizeof(RGBA8));

	VkImageMemoryBarrier barrier{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
	barrier.srcAccessMask = VK_ACCESS_NONE_KHR;
	barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barrier.srcQueueFamilyIndex = VK::graphicsFamily;
	barrier.dstQueueFamilyIndex = VK::graphicsFamily;
	barrier.image = tex.image;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;
	VK::vkCmdPipelineBarrier(stagingCmdBuf, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

	if (data) {
		VK::graphicsStager.upload_to_image(tex.image, data, width, height, sizeof(RGBA8), 0);

		barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		barrier.dstAccessMask = VK_ACCESS_NONE_KHR;
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		VK::vkCmdPipelineBarrier(stagingCmdBuf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
	}

	VkImageViewCreateInfo imageViewCreateInfo{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
	imageViewCreateInfo.image = tex.image;
	imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	imageViewCreateInfo.format = imageCreateInfo.format;
	imageViewCreateInfo.components = VkComponentMapping{ VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
	imageViewCreateInfo.subresourceRange = VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS };
	CHK_VK(VK::vkCreateImageView(VK::logicalDevice, &imageViewCreateInfo, nullptr, &tex.imageView));

	allTextures.push_back(result);

	if (currentTextureCount == currentTextureMaxCount) {
		currentTextureMaxCount *= 2;
		VK::drawDataDescriptorSet.change_dynamic_array_length(currentTextureMaxCount);
	}

	VkWriteDescriptorSet write{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
	write.dstSet = VK::drawDataDescriptorSet.descriptorSet;
	write.dstBinding = VK::drawDataDescriptorSet.bindingCount - 1;
	write.dstArrayElement = currentTextureCount;
	write.descriptorCount = 1;
	write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
	VkDescriptorImageInfo imageInfo;
	imageInfo.sampler = VK_NULL_HANDLE;
	imageInfo.imageView = tex.imageView;
	imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	write.pImageInfo = &imageInfo;
	VK::vkUpdateDescriptorSets(VK::logicalDevice, 1, &write, 0, nullptr);

	tex.index = currentTextureCount;
	currentTextureCount++;
}

void load_png(Texture* result, StrA path) {
	MemoryArena& stackArena = get_scratch_arena();
	MEMORY_ARENA_FRAME(stackArena) {
		RGBA8* image;
		U32 width, height;
		PNG::read_image(stackArena, &image, &width, &height, path);
		if (image) {
			create_texture(result, image, width, height, TEXTURE_FORMAT_RGBA_U8, TEXTURE_TYPE_COLOR);
		} else {
			*result = missing;
		}
	}
}

void load_msdf(Texture* result, StrA path) {
	MemoryArena& stackArena = get_scratch_arena();
	MEMORY_ARENA_FRAME(stackArena) {
		U32 fileSize = 0;
		Byte* file = read_full_file_to_arena<Byte>(&fileSize, stackArena, path);
		if (file) {
			U32 width = LOAD_LE32(file);
			U32 height = LOAD_LE32(file + sizeof(U32));
			create_texture(result, file + sizeof(U32) * 2, width, height, TEXTURE_FORMAT_RGBA_U8, TEXTURE_TYPE_MSDF);
		} else {
			*result = missing;
		}
		
	}
}

void load_all() {
	allTextures.reserve(256);
	memoryUsed.reserve(256);

	RGBA8 hardcodedTextureData[16 * 16];
	// The classic purple and black checker pattern
	for (U32 y = 0; y < 16; y++) {
		for (U32 x = 0; x < 16; x++) {
			hardcodedTextureData[y * 16  + x] = x < 8 == y < 8 ? RGBA8{255, 0, 255, 255} : RGBA8{0, 0, 0, 255};
		}
	}
	create_texture(&missing, hardcodedTextureData, 16, 16, TEXTURE_FORMAT_RGBA_U8, TEXTURE_TYPE_COLOR);
	memset(hardcodedTextureData, 0xFF, sizeof(hardcodedTextureData));
	create_texture(&simpleWhite, hardcodedTextureData, 16, 16, TEXTURE_FORMAT_RGBA_U8, TEXTURE_TYPE_COLOR);
	load_png(&dropdownUp, "resources/textures/dropdown_up.png"sa);
	load_png(&dropdownDown, "resources/textures/dropdown_down.png"sa);
	load_png(&uiX, "resources/textures/ui_x.png"sa);
	load_png(&nodeConnect, "resources/textures/node_connect.png"sa);
	load_msdf(&fontAtlas, "resources/textures/font.ddf"sa);
}

void destroy_all() {
	for (Texture* tex : allTextures) {
		VK::vkDestroyImageView(VK::logicalDevice, tex->imageView, nullptr);
		VK::vkDestroyImage(VK::logicalDevice, tex->image, nullptr);
	}
	allTextures.clear();
	for (VkDeviceMemory mem : memoryUsed) {
		VK::vkFreeMemory(VK::logicalDevice, mem, nullptr);
	}
	memoryUsed.clear();
}

}