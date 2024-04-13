#version 460
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_buffer_reference_uvec2 : require
#extension GL_EXT_scalar_block_layout : require

struct Vertex {
	vec3 pos;
	vec2 tex;
	uint color;
	uint textureIndex;
	uint flags;
};

layout (scalar, buffer_reference, buffer_reference_align = 16) readonly buffer VertexData {
	Vertex data[];
};
layout (scalar, buffer_reference, buffer_reference_align = 16) readonly buffer ClipBoxData {
	vec4 data[];
};

layout (push_constant) uniform RenderData {
	vec2 screenDimensions;
	uvec2 vertexBufferPointer;
	uvec2 clipBoxesPointer;
} renderData;

layout (location = 0) out vec2 pos;
layout (location = 1) out vec2 texcoord;
layout (location = 2) out vec4 color;
layout (location = 3) flat out uint texidx;
layout (location = 4) flat out uint flags;
layout (location = 5) flat out vec4 clipBox;

void main(){
	Vertex vert = VertexData(renderData.vertexBufferPointer).data[gl_VertexIndex];
	pos = vert.pos.xy;
	texcoord = vert.tex;
	color = unpackUnorm4x8(vert.color);
	texidx = vert.textureIndex;
	flags = vert.flags;
	if (renderData.clipBoxesPointer != uvec2(0, 0)) {
		clipBox = ClipBoxData(renderData.clipBoxesPointer).data[vert.flags >> 16];
	}
	gl_Position = vec4((vert.pos.xy / renderData.screenDimensions) * 2.0 - 1.0, 1.0 / vert.pos.z, 1.0);
}