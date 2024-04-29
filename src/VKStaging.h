#pragma once
#include "DrillLib.h"
#include "VK_decl.h"

namespace VKStaging {

struct StagingBuffer {
	VkBuffer uploadBuffer;
	void* memoryMapping;
	VkCommandPool commandPool;
	VkCommandBuffer cmdBuffer;
	VkFence uploadFinishedFence;
	U32 offset;
	B32 submitted;
};

U32 uploadStagingBufferSize = 8 * MEGABYTE;

struct GPUUploadStager {
	VkQueue queue;
	U32 queueFamily;
	VkDeviceMemory memory;
	StagingBuffer stagingBuffers[2];
	U32 currentBufferIdx;

	void init(VkQueue queueToUse, U32 queueFamilyIdx) {
		queue = queueToUse;
		queueFamily = queueFamilyIdx;
		currentBufferIdx = 0;

		VkMemoryRequirements memoryRequirements[2];
		for (U32 i = 0; i < 2; i++) {
			StagingBuffer& stagingBuffer = stagingBuffers[i];
			VkBufferCreateInfo bufferInfo{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
			bufferInfo.flags = 0;
			bufferInfo.size = uploadStagingBufferSize;
			bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
			bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			bufferInfo.queueFamilyIndexCount = 1;
			bufferInfo.pQueueFamilyIndices = &queueFamilyIdx;
			CHK_VK(VK::vkCreateBuffer(VK::logicalDevice, &bufferInfo, VK_NULL_HANDLE, &stagingBuffer.uploadBuffer));
			VK::vkGetBufferMemoryRequirements(VK::logicalDevice, stagingBuffer.uploadBuffer, &memoryRequirements[i]);
			if (!(memoryRequirements[i].memoryTypeBits & (1 << VK::hostMemoryTypeIndex))) {
				abort("Could not create upload buffer in host visible memory");
			}

			VkCommandPoolCreateInfo cmdPoolInfo{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
			cmdPoolInfo.flags = 0;
			cmdPoolInfo.queueFamilyIndex = queueFamilyIdx;
			CHK_VK(VK::vkCreateCommandPool(VK::logicalDevice, &cmdPoolInfo, VK_NULL_HANDLE, &stagingBuffer.commandPool));
			VkCommandBufferAllocateInfo cmdBufInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
			cmdBufInfo.commandPool = stagingBuffer.commandPool;
			cmdBufInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
			cmdBufInfo.commandBufferCount = 1;
			CHK_VK(VK::vkAllocateCommandBuffers(VK::logicalDevice, &cmdBufInfo, &stagingBuffer.cmdBuffer));

			VkCommandBufferBeginInfo beginInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
			beginInfo.flags = 0;
			beginInfo.pInheritanceInfo = nullptr;
			CHK_VK(VK::vkBeginCommandBuffer(stagingBuffer.cmdBuffer, &beginInfo));

			VkFenceCreateInfo fenceInfo{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
			fenceInfo.flags = 0;
			CHK_VK(VK::vkCreateFence(VK::logicalDevice, &fenceInfo, VK_NULL_HANDLE, &stagingBuffer.uploadFinishedFence));
		}

		VkMemoryAllocateInfo memoryAllocateInfo{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
		memoryAllocateInfo.allocationSize = ALIGN_HIGH(memoryRequirements[0].size, memoryRequirements[1].alignment) + memoryRequirements[1].size;
		memoryAllocateInfo.memoryTypeIndex = VK::hostMemoryTypeIndex;
		CHK_VK(VK::vkAllocateMemory(VK::logicalDevice, &memoryAllocateInfo, nullptr, &memory));
		void* memoryMapping;
		CHK_VK(VK::vkMapMemory(VK::logicalDevice, memory, 0, memoryAllocateInfo.allocationSize, 0, &memoryMapping));

		U32 mappedDataOffset = 0;
		for (U32 i = 0; i < 2; i++) {
			mappedDataOffset = ALIGN_HIGH(mappedDataOffset, memoryRequirements[i].alignment);
			CHK_VK(VK::vkBindBufferMemory(VK::logicalDevice, stagingBuffers[i].uploadBuffer, memory, mappedDataOffset));
			stagingBuffers[i].memoryMapping = reinterpret_cast<Byte*>(memoryMapping) + mappedDataOffset;
			mappedDataOffset += U32(memoryRequirements[i].size);
		}
	}

	void destroy() {
		for (U32 i = 0; i < 2; i++) {
			VK::vkDestroyCommandPool(VK::logicalDevice, stagingBuffers[i].commandPool, nullptr);
			VK::vkDestroyBuffer(VK::logicalDevice, stagingBuffers[i].uploadBuffer, nullptr);
			VK::vkDestroyFence(VK::logicalDevice, stagingBuffers[i].uploadFinishedFence, nullptr);
		}
		VK::vkUnmapMemory(VK::logicalDevice, memory);
		VK::vkFreeMemory(VK::logicalDevice, memory, nullptr);
	}

	VkCommandBuffer current_cmd_buf() {
		return stagingBuffers[currentBufferIdx].cmdBuffer;
	}

	void upload_to_buffer(VkBuffer dst, U64 dstOffset, void* src, U64 size) {
		while (size) {
			StagingBuffer& stagingBuffer = stagingBuffers[currentBufferIdx];
			wait_for_staging_buffer(stagingBuffer);
			U32 amountToCopy = U32(min<U64>(size, uploadStagingBufferSize - stagingBuffer.offset));
			memcpy(reinterpret_cast<Byte*>(stagingBuffer.memoryMapping) + stagingBuffer.offset, src, amountToCopy);
			VkBufferCopy region{};
			region.srcOffset = stagingBuffer.offset;
			region.dstOffset = dstOffset;
			region.size = amountToCopy;
			VK::vkCmdCopyBuffer(stagingBuffer.cmdBuffer, stagingBuffer.uploadBuffer, dst, 1, &region);
			stagingBuffer.offset += amountToCopy;
			dstOffset += amountToCopy;
			src = reinterpret_cast<Byte*>(src) + amountToCopy;
			size -= amountToCopy;
			if (stagingBuffer.offset == uploadStagingBufferSize) {
				flush();
			}
		}
	}

	VkCommandBuffer prepare_for_image_upload(U32 width, U32 height, U32 bytesPerTexel) {
		U32 size = width * height * bytesPerTexel;
		if (size > uploadStagingBufferSize) {
			print("Image upload was too large for staging buffer, resizing\n");
			flush();
			wait_for_staging_buffer(stagingBuffers[0]);
			wait_for_staging_buffer(stagingBuffers[1]);
			destroy();
			uploadStagingBufferSize = next_power_of_two(size);
			init(queue, queueFamily);
		}
		if (uploadStagingBufferSize - stagingBuffers[currentBufferIdx].offset < size) {
			flush();
		}
		StagingBuffer& stagingBuffer = stagingBuffers[currentBufferIdx];
		wait_for_staging_buffer(stagingBuffer);
		return stagingBuffer.cmdBuffer;
	}

	void upload_to_image(VkImage dst, void* src, U32 width, U32 height, U32 bytesPerTexel, U32 mipLevel) {
		U32 size = width * height * bytesPerTexel;
		if (size > uploadStagingBufferSize) {
			print("Image upload was too large for staging buffer, resizing\n");
			flush();
			wait_for_staging_buffer(stagingBuffers[0]);
			wait_for_staging_buffer(stagingBuffers[1]);
			destroy();
			uploadStagingBufferSize = next_power_of_two(size);
			init(queue, queueFamily);
		}
		if (uploadStagingBufferSize - stagingBuffers[currentBufferIdx].offset < size) {
			flush();
		}
		StagingBuffer& stagingBuffer = stagingBuffers[currentBufferIdx];
		wait_for_staging_buffer(stagingBuffer);
		memcpy(reinterpret_cast<Byte*>(stagingBuffer.memoryMapping) + stagingBuffer.offset, src, size);
		VkBufferImageCopy cpy{};
		cpy.bufferOffset = stagingBuffer.offset;
		// 0 means tightly packed
		cpy.bufferRowLength = 0;
		cpy.bufferImageHeight = 0;
		cpy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		cpy.imageSubresource.mipLevel = mipLevel;
		cpy.imageSubresource.baseArrayLayer = 0;
		cpy.imageSubresource.layerCount = 1;
		cpy.imageOffset = VkOffset3D{ 0, 0, 0 };
		cpy.imageExtent = VkExtent3D{ width, height, 1 };
		VK::vkCmdCopyBufferToImage(stagingBuffer.cmdBuffer, stagingBuffer.uploadBuffer, dst, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &cpy);
		stagingBuffer.offset += size;
	}

	void flush() {
		StagingBuffer& stagingBuffer = stagingBuffers[currentBufferIdx];
		if (!stagingBuffer.submitted && stagingBuffer.offset != 0) {
			if (!(VK::hostMemoryFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
				VkMappedMemoryRange memoryInvalidateRange{ VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE };
				memoryInvalidateRange.memory = memory;
				memoryInvalidateRange.offset = VkDeviceSize(currentBufferIdx) * uploadStagingBufferSize;
				memoryInvalidateRange.size = min<VkDeviceSize>(ALIGN_HIGH(stagingBuffer.offset, VK::physicalDeviceProperties.limits.nonCoherentAtomSize), uploadStagingBufferSize);
				CHK_VK(VK::vkFlushMappedMemoryRanges(VK::logicalDevice, 1, &memoryInvalidateRange));
			}
			
			CHK_VK(VK::vkEndCommandBuffer(stagingBuffer.cmdBuffer));

			VkSubmitInfo submitInfo{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
			submitInfo.waitSemaphoreCount = 0;
			submitInfo.pWaitSemaphores = nullptr;
			submitInfo.pWaitDstStageMask = nullptr;
			submitInfo.commandBufferCount = 1;
			submitInfo.pCommandBuffers = &stagingBuffer.cmdBuffer;
			submitInfo.signalSemaphoreCount = 0;
			submitInfo.pSignalSemaphores = nullptr;
			CHK_VK(VK::vkQueueSubmit(queue, 1, &submitInfo, stagingBuffer.uploadFinishedFence));

			stagingBuffer.submitted = true;
			currentBufferIdx = (currentBufferIdx + 1) & 1;
		}
	}

	void wait_for_staging_buffer(StagingBuffer& stagingBuffer) {
		if (stagingBuffer.submitted) {
			CHK_VK(VK::vkWaitForFences(VK::logicalDevice, 1, &stagingBuffer.uploadFinishedFence, VK_TRUE, U64_MAX));
			CHK_VK(VK::vkResetFences(VK::logicalDevice, 1, &stagingBuffer.uploadFinishedFence));
			CHK_VK(VK::vkResetCommandPool(VK::logicalDevice, stagingBuffer.commandPool, 0));
			stagingBuffer.submitted = false;
			stagingBuffer.offset = 0;
			VkCommandBufferBeginInfo beginInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
			beginInfo.flags = 0;
			beginInfo.pInheritanceInfo = nullptr;
			CHK_VK(VK::vkBeginCommandBuffer(stagingBuffer.cmdBuffer, &beginInfo));
		}
	}
};

}