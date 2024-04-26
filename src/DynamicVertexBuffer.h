#pragma once
#include "DrillLib.h"
#include "DynamicVertexBuffer_decl.h"
#include "VK.h"

namespace DynamicVertexBuffer {

struct DrawCommand {
	VkPipeline pipeline;
	VkPipelineLayout pipelineLayout;
	U32 vertexBufferOffset;
	U32 indexBufferOffset;
	U32 vertexCount;
	U32 indexCount;
	U64 clipBoxesGPUAddress;
};

enum DrawMode : U32 {
	DRAW_MODE_TRIANGLES,
	DRAW_MODE_QUADS
};

struct Tessellator {
	VK::DedicatedBuffer buffer;
	U32 vertexCapacity;
	U32 indexCapacity;
	U32 currentVertexByteCount;
	U32 currentIndexByteCount;
	Byte* vertexDataPointer;
	U32* indexDataPointer;
	B32 isCurrentlyDrawing;
	DrawMode currentDrawMode;
	ArenaArrayList<DrawCommand> drawCommands;

	void ensure_space_for(U32 vertexBytes, U32 indexBytes) {
		if (currentVertexByteCount + vertexBytes <= vertexCapacity && currentIndexByteCount + indexBytes <= indexCapacity) {
			return;
		}
		VK::DedicatedBuffer oldBuffer = buffer;
		Byte* oldVertexDataPointer = vertexDataPointer;
		U32* oldIndexDataPointer = indexDataPointer;

		vertexCapacity = next_power_of_two(currentVertexByteCount + vertexBytes);
		indexCapacity = next_power_of_two(currentIndexByteCount + indexBytes);

		buffer.create(vertexCapacity + indexCapacity, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VK::hostMemoryTypeIndex);
		vertexDataPointer = reinterpret_cast<Byte*>(buffer.mapping);
		indexDataPointer = reinterpret_cast<U32*>(vertexDataPointer + vertexCapacity);

		if (oldVertexDataPointer) {
			memcpy(vertexDataPointer, oldVertexDataPointer, currentVertexByteCount);
			memcpy(indexDataPointer, oldIndexDataPointer, currentIndexByteCount);
			oldBuffer.destroy();
		}
	}

	void init() {
		buffer = {};
		vertexDataPointer = nullptr;
		indexDataPointer = nullptr;
		currentVertexByteCount = 0;
		currentIndexByteCount = 0;
		isCurrentlyDrawing = false;
		drawCommands = ArenaArrayList<DrawCommand>{};
		ensure_space_for(4 * MEGABYTE, 1 * MEGABYTE);

	}
	void destroy() {
		buffer.destroy();
	}

	void begin_draw(VkPipeline pipeline, VkPipelineLayout pipelineLayout, DrawMode mode) {
		if (isCurrentlyDrawing) {
			abort("Already drawing something, end that draw first");
		}
		isCurrentlyDrawing = true;
		currentDrawMode = mode;
		drawCommands.allocator = &frameArena;
		currentVertexByteCount = ALIGN_HIGH(currentVertexByteCount, 16);
		DrawCommand& drawCmd = drawCommands.push_back();
		drawCmd.pipeline = pipeline;
		drawCmd.pipelineLayout = pipelineLayout;
		drawCmd.indexBufferOffset = currentIndexByteCount / sizeof(U32);
		drawCmd.vertexBufferOffset = currentVertexByteCount;
		drawCmd.vertexCount = 0;
		drawCmd.indexCount = 0;
		drawCmd.clipBoxesGPUAddress = 0;
	}
	void set_clip_boxes(U64 address) {
		drawCommands.back().clipBoxesGPUAddress = address;
	}
	void end_draw() {
		isCurrentlyDrawing = false;
	}
	void draw() {
		buffer.invalidate_mapped();

		VK::UIPipelineRenderData renderData;
		renderData.screenDimensions = V2F32{ F32(VK::desktopSwapchainData.width), F32(VK::desktopSwapchainData.height) };
		VK::vkCmdBindIndexBuffer(VK::graphicsCommandBuffer, buffer.buffer, vertexCapacity, VK_INDEX_TYPE_UINT32);
		VkPipeline prevPipeline = VK_NULL_HANDLE;
		for (DrawCommand& drawCmd : drawCommands) {
			if (drawCmd.pipeline != prevPipeline) {
				VK::vkCmdBindPipeline(VK::graphicsCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, drawCmd.pipeline);
				VK::vkCmdBindDescriptorSets(VK::graphicsCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, drawCmd.pipelineLayout, 0, 1, &VK::drawDataDescriptorSet.descriptorSet, 0, nullptr);
			}
			renderData.vertexBufferPointer = buffer.gpuAddress + drawCmd.vertexBufferOffset;
			renderData.clipBoxesPointer = drawCmd.clipBoxesGPUAddress;
			VK::vkCmdPushConstants(VK::graphicsCommandBuffer, drawCmd.pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(VK::UIPipelineRenderData), &renderData);
			VK::vkCmdDrawIndexed(VK::graphicsCommandBuffer, drawCmd.indexCount, 1, drawCmd.indexBufferOffset, 0, 0);
		}
		drawCommands = ArenaArrayList<DrawCommand>{};
		currentIndexByteCount = 0;
		currentVertexByteCount = 0;
	}

	Tessellator& pos3(F32 x, F32 y, F32 z) {
		STORE_LE32(vertexDataPointer + currentVertexByteCount, bitcast<U32>(x));
		STORE_LE32(vertexDataPointer + currentVertexByteCount + 4, bitcast<U32>(y));
		STORE_LE32(vertexDataPointer + currentVertexByteCount + 8, bitcast<U32>(z));
		currentVertexByteCount += 3 * sizeof(F32);
		return *this;
	}
	Tessellator& pos2(F32 x, F32 y) {
		STORE_LE32(vertexDataPointer + currentVertexByteCount, bitcast<U32>(x));
		STORE_LE32(vertexDataPointer + currentVertexByteCount + 4, bitcast<U32>(y));
		currentVertexByteCount += 2 * sizeof(F32);
		return *this;
	}
	Tessellator& tex(F32 u, F32 v) {
		STORE_LE32(vertexDataPointer + currentVertexByteCount, bitcast<U32>(u));
		STORE_LE32(vertexDataPointer + currentVertexByteCount + 4, bitcast<U32>(v));
		currentVertexByteCount += 2 * sizeof(F32);
		return *this;
	}
	Tessellator& color(F32 r, F32 g, F32 b, F32 a) {
		U32 color = pack_unorm4x8(V4F32{ r, g, b, a });
		STORE_LE32(vertexDataPointer + currentVertexByteCount, color);
		currentVertexByteCount += sizeof(U32);
		return *this;
	}
	Tessellator& color(F32 r, F32 g, F32 b) {
		return color(r, g, b, 1.0F);
	}
	Tessellator& u32(U32 v) {
		STORE_LE32(vertexDataPointer + currentVertexByteCount, v);
		currentVertexByteCount += sizeof(U32);
		return *this;
	}
	Tessellator& tex_idx(U32 v) {
		return u32(v);
	}
	Tessellator& flags(U32 v) {
		return u32(v);
	}
	template<typename T>
	Tessellator& element(T& element) {
		memcpy(vertexDataPointer + currentVertexByteCount, &element, sizeof(element));
		currentVertexByteCount += sizeof(element);
		return *this;
	}
	Tessellator& end_vert() {
		DrawCommand& cmd = drawCommands.back();
		cmd.vertexCount++;
		U32 indexOffset = currentIndexByteCount / 4;
		if (currentDrawMode == DRAW_MODE_QUADS) {
			if ((cmd.vertexCount & 0b11) == 0) {
				indexDataPointer[indexOffset] = cmd.vertexCount - 4;
				indexDataPointer[indexOffset + 1] = cmd.vertexCount - 3;
				indexDataPointer[indexOffset + 2] = cmd.vertexCount - 2;
				indexDataPointer[indexOffset + 3] = cmd.vertexCount - 4;
				indexDataPointer[indexOffset + 4] = cmd.vertexCount - 2;
				indexDataPointer[indexOffset + 5] = cmd.vertexCount - 1;
				cmd.indexCount += 6;
				currentIndexByteCount += 6 * sizeof(U32);
			}
		} else { // DRAW_MODE_TRIANGLES
			indexDataPointer[indexOffset] = cmd.indexCount++;
			currentIndexByteCount += sizeof(U32);
		}
		ensure_space_for(1024, 6 * sizeof(U32));
		return *this;
	}
	template<typename Vertex>
	Tessellator& add_geometry(Vertex* vertices, U32 numVerts, U32* indices, U32 numIndices) {
		DrawCommand& cmd = drawCommands.back();
		ensure_space_for(numVerts * sizeof(Vertex), numIndices * sizeof(U32));
		memcpy(vertexDataPointer + currentVertexByteCount, vertices, numVerts * sizeof(Vertex));
		U32 indexOffset = currentIndexByteCount / 4;
		for (U32 i = 0; i < numIndices; i++) {
			indexDataPointer[indexOffset + i] = indices[i] + cmd.vertexCount;
		}
		currentVertexByteCount += numVerts * sizeof(Vertex);
		currentIndexByteCount += numIndices * sizeof(U32);
		cmd.vertexCount += numVerts;
		cmd.indexCount += numIndices;
		return *this;
	}
	Tessellator& ui_rect2d(F32 xStart, F32 yStart, F32 xEnd, F32 yEnd, F32 z, F32 uStart, F32 vStart, F32 uEnd, F32 vEnd, V4F32 color, U32 textureIndex, U32 flags) {

		DrawCommand& cmd = drawCommands.back();
		ensure_space_for(4 * sizeof(VK::UIVertex), 6 * sizeof(U32));

		U32 packedColor = pack_unorm4x8(color);
		VK::UIVertex vertices[4]{
			{ V3F32{ xStart, yStart, z }, V2F32{ uStart, vStart }, packedColor, textureIndex, flags },
			{ V3F32{ xStart, yEnd, z }, V2F32{ uStart, vEnd }, packedColor, textureIndex, flags },
			{ V3F32{ xEnd, yEnd, z }, V2F32{ uEnd, vEnd }, packedColor, textureIndex, flags },
			{ V3F32{ xEnd, yStart, z }, V2F32{ uEnd, vStart }, packedColor, textureIndex, flags },
		};

		memcpy(vertexDataPointer + currentVertexByteCount, vertices, 4 * sizeof(VK::UIVertex));
		U32 indexOffset = currentIndexByteCount / 4;
		indexDataPointer[indexOffset + 0] = cmd.vertexCount + 0;
		indexDataPointer[indexOffset + 1] = cmd.vertexCount + 1;
		indexDataPointer[indexOffset + 2] = cmd.vertexCount + 2;
		indexDataPointer[indexOffset + 3] = cmd.vertexCount + 0;
		indexDataPointer[indexOffset + 4] = cmd.vertexCount + 2;
		indexDataPointer[indexOffset + 5] = cmd.vertexCount + 3;

		currentVertexByteCount += 4 * sizeof(VK::UIVertex);
		currentIndexByteCount += 6 * sizeof(U32);
		cmd.vertexCount += 4;
		cmd.indexCount += 6;
		return *this;
	}
	Tessellator& ui_line_strip(V2F32* points, U32 pointCount, F32 z, F32 thickness, V4F32 color, U32 textureIndex, U32 flags) {
		if (pointCount < 2) {
			return *this;
		}
		DrawCommand& cmd = drawCommands.back();
		U32 packedColor = pack_unorm4x8(color);
		U32 vertexCount = pointCount * 2;
		U32 indexCount = pointCount * 6 - 6;
		F32 scale = thickness * 0.5F;
		MemoryArena& stackArena = get_scratch_arena();
		MEMORY_ARENA_FRAME(stackArena) {
			VK::UIVertex* vertices = stackArena.alloc<VK::UIVertex>(vertexCount);
			// There's a better shape I should be using here that uses 4 tris per segment and mitigates a lot of distortion
			V2F32 toNext = normalize(points[1] - points[0]);
			V2F32 toPrev{};
			V2F32 orthogonal = get_orthogonal(toNext) * scale;
			V2F32 tex0{ 0.0F, 0.0F };
			V2F32 tex1{ 0.0F, 1.0F };
			V2F32 pos0 = points[0] - orthogonal;
			V2F32 pos1 = points[0] + orthogonal;
			vertices[0] = VK::UIVertex{ V3F32{ pos0.x, pos0.y, z}, tex0, packedColor, textureIndex, flags };
			vertices[1] = VK::UIVertex{ V3F32{ pos1.x, pos1.y, z}, tex1, packedColor, textureIndex, flags };
			for (U32 pointIdx = 1, vertIdx = 2; pointIdx < pointCount - 1; pointIdx++, vertIdx += 2) {
				V2F32 pos = points[pointIdx];
				toNext = normalize(points[pointIdx + 1] - pos);
				toPrev = normalize(points[pointIdx - 1] - pos);
				V2F32 direction = normalize(toNext + toPrev);
				F32 scaleA = 0.0F;
				F32 scaleB = 0.0F;
				if (absf32(cross(toNext, toPrev)) < 0.0001F) {
					direction = V2F32{ -toNext.y, toNext.x };
					scaleA = scaleB = scale;
				} else {
					scaleA = scale / absf32(dot(toNext, get_orthogonal(direction)));
					scaleA = min(scaleA, length(points[pointIdx - 1] - pos), length(points[pointIdx + 1] - pos));
					scaleB = scale / max(absf32(dot(toNext, get_orthogonal(direction))), 0.2F);
				}
				if (cross(direction, toNext) < 0.0F) {
					direction = -direction;
					swap(&scaleA, &scaleB);
				}

				F32 t = F32(pointIdx) / F32(pointCount - 1);
				tex0.x = t;
				tex1.x = t;
				pos0 = pos + direction * scaleA;
				pos1 = pos - direction * scaleB;
				vertices[vertIdx + 0] = VK::UIVertex{ V3F32{ pos0.x, pos0.y, z}, tex0, packedColor, textureIndex, flags };
				vertices[vertIdx + 1] = VK::UIVertex{ V3F32{ pos1.x, pos1.y, z}, tex1, packedColor, textureIndex, flags };
			}
			toNext = normalize(points[pointCount - 1] - points[pointCount - 2]);
			orthogonal = V2F32{ -toNext.y * scale, toNext.x * scale };
			tex0.x = 1.0F;
			tex1.x = 1.0F;
			pos0 = points[pointCount - 1] - orthogonal;
			pos1 = points[pointCount - 1] + orthogonal;
			vertices[vertexCount - 2] = VK::UIVertex{ V3F32{ pos0.x, pos0.y, z}, tex0, packedColor, textureIndex, flags};
			vertices[vertexCount - 1] = VK::UIVertex{ V3F32{ pos1.x, pos1.y, z}, tex1, packedColor, textureIndex, flags };

			ensure_space_for(vertexCount * sizeof(VK::UIVertex), indexCount * sizeof(U32));

			memcpy(vertexDataPointer + currentVertexByteCount, vertices, vertexCount * sizeof(VK::UIVertex));
			U32 indexOffset = currentIndexByteCount / 4;
			U32* indexPtr = indexDataPointer + indexOffset;
			for (U32 i = 0, vertexIdx = cmd.vertexCount; i < indexCount; i += 6, vertexIdx += 2) {
				indexPtr[i + 0] = vertexIdx + 0;
				indexPtr[i + 1] = vertexIdx + 3;
				indexPtr[i + 2] = vertexIdx + 2;
				indexPtr[i + 3] = vertexIdx + 0;
				indexPtr[i + 4] = vertexIdx + 1;
				indexPtr[i + 5] = vertexIdx + 3;
			}
			currentVertexByteCount += vertexCount * sizeof(VK::UIVertex);
			currentIndexByteCount += indexCount * sizeof(U32);
			cmd.vertexCount += vertexCount;
			cmd.indexCount += indexCount;
		}
		return *this;
	}
	Tessellator& ui_bezier_curve(V2F32 start, V2F32 controlA, V2F32 controlB, V2F32 end, F32 z, U32 subdivisions, F32 thickness, V4F32 color, U32 textureIndex, U32 flags) {
		subdivisions = max(subdivisions, 2u);
		MemoryArena& stackArena = get_scratch_arena();
		MEMORY_ARENA_FRAME(stackArena) {
			V2F32* positions = stackArena.alloc<V2F32>(subdivisions);
			for (U32 i = 0; i < subdivisions; i++) {
				F32 t = F32(i) / F32(subdivisions - 1);
				positions[i] = eval_bezier_cubic(start, controlA, controlB, end, t);
			}
			ui_line_strip(positions, subdivisions, z, thickness, color, textureIndex, flags);
		}
		return *this;
	}
} tessellators[VK::FRAMES_IN_FLIGHT];

void init() {
	for (U32 i = 0; i < VK::FRAMES_IN_FLIGHT; i++) {
		tessellators[i].init();
	}
}
void destroy() {
	for (U32 i = 0; i < VK::FRAMES_IN_FLIGHT; i++) {
		tessellators[i].destroy();
	}
}

Tessellator& get_tessellator() {
	return tessellators[VK::currentFrameInFlight];
}

}