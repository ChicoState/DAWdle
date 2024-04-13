#pragma once
#include "DrillLib.h"
#include "DynamicVertexBuffer.h"
#include "Textures.h"

namespace TextRenderer {

F32 string_size_x(StrA str, F32 sizeY) {
	F32 characterSpacing = 1.0F;
	F32 sizeX = sizeY * 0.5F;
	F32 characterStep = sizeX * ((6.0F + characterSpacing) / 6.0F);
	return characterStep * F32(str.length);
}

void draw_string_batched(DynamicVertexBuffer::Tessellator& tes, StrA str, F32 x, F32 y, F32 z, F32 sizeY, V4F32 color, U32 flags) {
	// Font characters are assumed to be 12 units tall and 6 units wide
	// I'm only going to handle monospaced right now because it's easier and I like monospaced fonts more anyway

	//TODO don't hard code this
	F32 radius = 12.0F / 64.0F;
	F32 height = 1.0F;
	F32 width = 0.5F;
	F32 offsetX = 0.25F;
	F32 offsetY = 0.0F;
	F32 scaledWidth = width * (1.0F / 16.0F);
	F32 scaledHeight = height * (1.0F / 16.0F);

	F32 characterSpacing = 1.0F;
	F32 sizeX = sizeY * 0.5F;
	F32 dstRadius = radius * sizeY;
	F32 characterStep = sizeX * ((6.0F + characterSpacing) / 6.0F);

	x -= dstRadius;
	y -= dstRadius;
	sizeX += dstRadius * 2.0F;
	sizeY += dstRadius * 2.0F;

	VK::UIVertex vertices[4];
	U32 quadIndices[6]{ 0, 1, 2, 0, 2, 3 };
	U32 packedColor = pack_unorm4x8(color);
	for (U32 i = 0; i < ARRAY_COUNT(vertices); i++) {
		vertices[i].color = packedColor;
		vertices[i].texIdx = Textures::fontAtlas.index;
		vertices[i].flags = flags | VK::UI_RENDER_FLAG_MSDF;
	}

	for (U64 i = 0; i < str.length; i++) {
		U32 glyph = U32(str.str[i]);
		F32 xStart = (F32(glyph & 0xF) + offsetX) * (1.0F / 16.0F);
		F32 yStart = (F32(glyph >> 4) + offsetY) * (1.0F / 16.0F);
		vertices[0].tex = V2F32{ xStart, yStart };
		vertices[1].tex = V2F32{ xStart, yStart + scaledHeight };
		vertices[2].tex = V2F32{ xStart + scaledWidth, yStart + scaledHeight };
		vertices[3].tex = V2F32{ xStart + scaledWidth, yStart };
		vertices[0].pos = V3F32{ x, y, z };
		vertices[1].pos = V3F32{ x, y + sizeY, z };
		vertices[2].pos = V3F32{ x + sizeX, y + sizeY, z };
		vertices[3].pos = V3F32{ x + sizeX, y, z };
		tes.add_geometry(vertices, 4, quadIndices, 6);

		x += characterStep;
	}
}

void draw_string(StrA str, F32 x, F32 y, F32 z, F32 sizeY, V4F32 color, U32 flags) {
	DynamicVertexBuffer::Tessellator& tes = DynamicVertexBuffer::get_tessellator();
	tes.begin_draw(VK::uiPipeline, VK::uiPipelineLayout, DynamicVertexBuffer::DRAW_MODE_QUADS);
	draw_string_batched(tes, str, x, y, z, sizeY, color, flags);
	tes.end_draw();
}

void draw_string(StrA str, F32 x, F32 y, F32 z, F32 sizeY) {
	draw_string(str, x, y, z, sizeY, V4F32{ 1.0F, 1.0F, 1.0F, 1.0F }, 0);
}

}