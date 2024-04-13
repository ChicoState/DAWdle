#version 460
#extension GL_EXT_nonuniform_qualifier : require

#define UI_RENDER_FLAG_MSDF 1

layout (set = 0, binding = 0) uniform sampler bilinearSampler;
layout (set = 0, binding = 1) uniform texture2D textures[];

layout (location = 0) out vec4 fragColor;

layout (location = 0) in vec2 pos;
layout (location = 1) in vec2 texcoord;
layout (location = 2) in vec4 color;
layout (location = 3) flat in uint texidx;
layout (location = 4) flat in uint flags;
layout (location = 5) flat in vec4 clipBox;

float median(float a, float b, float c){
	return max(min(a, b), min(max(a, b), c));
}

float screen_px_range(vec2 fragWidth){
	float sdfRadius = (12.0 / 64.0) / 16.0F;
	vec2 v = fragWidth / (sdfRadius * 2.0);
	return 0.5 * (v.x + v.y);
}

vec4 sample_color(float screenPxRange, vec2 coord){
	vec4 msdf = texture(sampler2D(textures[nonuniformEXT(texidx)], bilinearSampler), coord);
	float val = median(msdf.r, msdf.g, msdf.b);
	// Setting the cutoff to something a bit smaller makes the font look more bold,
	// but it also takes care of some artifacts I don't feel like fixing
	// (letter B has a hole and bad things happen at smaller font sizes)
	float cutoff = 0.45;
	return vec4(1, 1, 1, min(max((val - cutoff) / min(screenPxRange, 1 - cutoff), 0.0), 1.0));
}

void main(){
	if ((flags >> 16) != 0) {
		if (pos.x < clipBox.x || pos.x >= clipBox.z || pos.y < clipBox.y || pos.y >= clipBox.w) {
			discard;
		}
	}
	if ((flags & UI_RENDER_FLAG_MSDF) != 0) {
		vec2 texdx = dFdx(texcoord);
		vec2 texdy = dFdy(texcoord);
		float screenPxRange = screen_px_range(texdx + texdy);
		vec4 val = sample_color(screenPxRange, texcoord) * color;
		if(val.a == 0){
			discard;
		}
		fragColor = val;
	} else {
		fragColor = texture(sampler2D(textures[nonuniformEXT(texidx)], bilinearSampler), texcoord) * color;
	}
}