#pragma once

namespace Textures {

const U32 MAX_TEXTURE_COUNT = 128 * 1024;
U32 currentTextureMaxCount = 128;
U32 currentTextureCount = 0;

void load_all();
void destroy_all();

}