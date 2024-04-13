#pragma once
#include "DrillLib.h"

namespace PNG {

struct BitReader {
	U64 scratchValue;
	U32* data;
	U32 numBits;

	FINLINE void init(void* input) {
		Byte* bytes = reinterpret_cast<Byte*>(input);
		U32 byteReadCount = ((4 - (reinterpret_cast<uintptr_t>(bytes) & 3)) + 4);
		numBits = 0;
		scratchValue = 0;
		for (U32 i = 0; i < byteReadCount; i++) {
			scratchValue = scratchValue | (static_cast<U64>(*bytes) << numBits);
			numBits += 8;
			bytes++;
		}
		data = reinterpret_cast<U32*>(bytes);
	}

	FINLINE void try_read_next() {
		if (numBits <= 32) {
			scratchValue = (static_cast<U64>(*data++) << numBits) | scratchValue;
			numBits += 32;
		}
		/*U64 test = numBits <= 32;
		U64 mask = ~test + 1;
		scratchValue = ((static_cast<U64>(*data) << numBits) & mask) | scratchValue;
		data += test;
		numBits += test << 5;*/
	}

	// Assumes byte alignment. Call align_to_byte if unsure.
	FINLINE void increase_byte_pos(U32 amount) {
		RUNTIME_ASSERT((numBits & 0b111) == 0, "Bits not aligned! Data could be lost");
		// Clear the scratch value, restore those bytes to the data block, increase the data block pointer by specified bytes.
		U32 numBytesInBuffer = (numBits + 7) / 8;
		Byte* newData = reinterpret_cast<Byte*>(data);
		newData -= numBytesInBuffer;
		newData += amount;
		data = reinterpret_cast<U32*>(newData);
		// Put data back into the scratch block
		scratchValue = (static_cast<U64>(data[1]) << 32) | data[0];
		data += 2;
		numBits = 64;
	}

	FINLINE void align_to_byte() {
		// Take off the lowest 3 bits, 0-7, to align to 8 bits
		U32 oldNumBits = numBits;
		numBits &= ~0B111;
		scratchValue = scratchValue >> (oldNumBits - numBits);
		try_read_next();
	}

	FINLINE Byte* get_current_pointer() {
		U32 numBytesInBuffer = (numBits + 7) / 8;
		return reinterpret_cast<Byte*>(data) - numBytesInBuffer;
	}

	FINLINE U32 read_bits(U32 bitCount) {
		numBits -= bitCount;
		U32 val = static_cast<U32>(scratchValue) & ((1 << bitCount) - 1);
		scratchValue = scratchValue >> bitCount;
		try_read_next();
		return val;
	}

	FINLINE U32 read_bits_no_check(U32 bitCount) {
		numBits -= bitCount;
		U32 val = static_cast<U32>(scratchValue & ((1 << bitCount) - 1));
		scratchValue = scratchValue >> bitCount;
		return val;
	}

	FINLINE void put_back_bits(U32 bits, U32 bitCount) {
		numBits += bitCount;
		scratchValue = (scratchValue << bitCount) | (bits & ((1 << bitCount) - 1));
	}

	FINLINE U8 read_uint8() {
		numBits -= 8;
		U8 val = static_cast<U8>(scratchValue & ((~0ULL) >> 56));
		scratchValue = scratchValue >> 8;
		try_read_next();
		return val;
	}

	FINLINE U16 read_uint16() {
		numBits -= 16;
		U16 val = static_cast<U16>(scratchValue & ((~0ULL) >> 48));
		scratchValue = scratchValue >> 16;
		try_read_next();
		return val;
	}

	FINLINE U32 read_uint32() {
		return read_bits(32);
	}
};

struct HuffmanTree {
	constexpr static U32 MAX_BIT_LENGTH = 15;
	constexpr static U32 MAX_CODES = 286;
	//U16 lookupTable[32768];
	U16 lengthCounts[MAX_BIT_LENGTH + 1];
	U16 symbols[MAX_CODES * 2];

	inline U16 read_next(BitReader& reader) {
		/*U16 bits = reader.read_bits_no_check(MAX_BIT_LENGTH);
		U16 value = lookupTable[bits];
		U16 length = value >> 12;
		value &= 0xFFF;
		reader.put_back_bits(bits >> length, MAX_BIT_LENGTH-length);
		reader.try_read_next();
		return value;*/
		/*I32 symbolIndex = 0;
		I32 firstCodeForLength = 0;
		I32 huffmanCode = 0;

		for (I32 treeLevel = 1; treeLevel <= MAX_BIT_LENGTH; treeLevel++) {
			I32 bit = reader.read_bits(1);
			huffmanCode = (huffmanCode << 1) | bit;
			I32 count = lengthCounts[treeLevel];
			if ((huffmanCode - count) < firstCodeForLength) {
				I32 index = symbolIndex + (huffmanCode - firstCodeForLength);
				return symbols[index];
			}
			symbolIndex += count;
			firstCodeForLength = (firstCodeForLength + count) << 1;
		}
		constexpr U16 error = ~0;
		assert(false && "Huffman decode failed");
		return error;*/
		U32 scratch = reader.read_bits_no_check(15);
		I32 symbolIndex = 0;
		I32 firstCodeForLength = 0;
		I32 huffmanCode = 0;

		for (I32 treeLevel = 1; treeLevel <= MAX_BIT_LENGTH; treeLevel++) {
			I32 bit = I32(scratch & 1);
			scratch >>= 1;
			huffmanCode = (huffmanCode << 1) | bit;
			I32 count = lengthCounts[treeLevel];
			if ((huffmanCode - count) < firstCodeForLength) {
				I32 index = symbolIndex + (huffmanCode - firstCodeForLength);
				reader.put_back_bits(scratch, MAX_BIT_LENGTH - treeLevel);
				reader.try_read_next();
				return symbols[index];
			}
			symbolIndex += count;
			firstCodeForLength = (firstCodeForLength + count) << 1;
		}
		const U16 error = U16_MAX;
		RUNTIME_ASSERT(false, "Huffman decode failed");
		return error;
	}

	U16 reverse_bits(U16 inputBits) {
		U32 bits = inputBits;
		bits = ((bits & 0x00FF) << 8) | ((bits & 0xFF00) >> 8);
		bits = ((bits & 0x0F0F) << 4) | ((bits & 0xF0F0) >> 4);

		bits = ((bits & 0x3333) << 2) | ((bits & 0xCCCC) >> 2);
		bits = ((bits & 0x5555) << 1) | ((bits & 0xAAAA) >> 1);
		return U16(bits);
	}

	void build(U8* valueLengths, U32 numValues) {
		memset(lengthCounts, 0, (MAX_BIT_LENGTH + 1) * sizeof(U16));
		for (U32 i = 0; i < numValues; i++) {
			lengthCounts[valueLengths[i]]++;
		}
		lengthCounts[0] = 0;
		U32 offsets[MAX_BIT_LENGTH + 2];
		offsets[0] = 0;
		offsets[1] = 0;
		//U16 code = 0;
		//U32 nextCode[MAX_BIT_LENGTH+1];
		for (U32 i = 1; i <= MAX_BIT_LENGTH; i++) {
			offsets[i + 1] = offsets[i] + lengthCounts[i];
			//code = (code + lengthCounts[i - 1]) << 1;
			//nextCode[i] = code;
		}
		for (U16 value = 0; value < numValues; value++) {
			U8 valLength = valueLengths[value];
			if (valLength > 0) {
				/*U16 huffman = nextCode[valLength] << (MAX_BIT_LENGTH - valLength);
				nextCode[valLength]++;
				for (U32 fill = 0; fill < (1 << (MAX_BIT_LENGTH - valLength)); fill++) {
					U32 lookupIndex = reverse_bits(huffman + fill) >> 1;
					lookupTable[lookupIndex] = (valLength << 12) | value;
				}*/

				U32 offset = offsets[valLength];
				symbols[offset] = value;
				offsets[valLength]++;
			}
		}

	}
};

enum CompressionMethod {
	COMPRESSION_METHOD_NONE = 0,
	COMPRESSION_METHOD_DEFLATE = 8,
	COMPRESSION_METHOD_RESERVED = 15
};

enum CompressionLevel {
	COMPRESSION_LEVEL_FASTEST = 0,
	COMPRESSION_LEVEL_FAST = 1,
	COMPRESSION_LEVEL_DEFAULT = 2,
	COMPRESSION_LEVEL_MAXIMUM = 3
};

enum DeflateCompressionType {
	// No compression, stored as is
	DEFLATE_COMPRESSION_NONE = 0,
	// Compressed with a fixed huffman tree defined by spec
	DEFLATE_COMPRESSION_FIXED = 1,
	// Huffman tree is stored as well
	DEFLATE_COMPRESSION_DYNAMIC = 2,
	// Reserved for future use, error
	DEFLATE_COMPRESSION_RESERVED = 3
};

// Adler32 as described by the zlib format spec
FINLINE U32 adler32(Byte* data, U32 length) {
	U32 s1 = 1;
	U32 s2 = 0;
	// According to the spec, the mod only has to be run every 5552 bytes. That makes this function an order of magnitude faster.
	while (length >= 5552) {
		for (U32 i = 0; i < 5552; i++) {
			s1 += data[i];
			s2 = s2 + s1;
		}
		data += 5552;
		length -= 5552;
		s1 %= 65521;
		s2 %= 65521;
	}
	while (length--) {
		s1 += *data++;
		s2 = s2 + s1;
	}
	s1 %= 65521;
	s2 %= 65521;
	return (s2 << 16) | s1;
}

// CRC code based on the lib png sample
U32 crcTable[256];

FINLINE void compute_crc_table() {
	for (U32 n = 0; n < 256; n++) {
		U32 val = n;
		for (U32 k = 0; k < 8; k++) {
			if (val & 1) {
				val = 0xEDB88320UL ^ (val >> 1);
			} else {
				val = val >> 1;
			}
		}
		crcTable[n] = val;
	}
}

FINLINE U32 crc32(Byte* data, U32 length) {
	U64 crc = 0xFFFFFFFFULL;
	for (U32 i = 0; i < length; i++) {
		crc = crcTable[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
	}
	return U32(crc ^ 0xFFFFFFFFULL);
}

FINLINE void resize_buffer(MemoryArena& arena, Byte** buffer, U32* size, U32 usedBytes, U32 accomodateSize) {
	if (accomodateSize < *size) {
		return;
	}
	U32 oldSize = *size;
	while (accomodateSize >= *size) {
		*size += *size >> 1;
	}
	*buffer = arena.realloc<Byte>(*buffer, oldSize, *size);
}

void generate_fixed_tree(HuffmanTree& litlenFixedTree, HuffmanTree& distFixedTree) {
	U8 litLenValueLengths[288];
	memset(litLenValueLengths, 8, 144);
	memset(litLenValueLengths + 144, 9, 112);
	memset(litLenValueLengths + 256, 7, 24);
	memset(litLenValueLengths + 280, 8, 8);
	litlenFixedTree.build(litLenValueLengths, 288);

	U8 distVals[30];
	memset(distVals, 5, 30);
	distFixedTree.build(distVals, 30);
}

void read_tree_lengths(BitReader& reader, HuffmanTree& decompressTree, U32 numCodes, U8* codeLengths) {
	const U32 COPY3_6 = 16;
	const U32 ZERO3_10 = 17;
	const U32 ZERO11_138 = 18;

	for (U32 codeIdx = 0; codeIdx < numCodes;) {
		U8 length = static_cast<U8>(decompressTree.read_next(reader));
		if (length <= 15) {
			codeLengths[codeIdx++] = length;
		} else if (length == COPY3_6) {
			U8 copy = codeLengths[codeIdx - 1];
			U32 copyCount = 3 + reader.read_bits(2);
			for (U32 j = 0; j < copyCount; j++) {
				codeLengths[codeIdx++] = copy;
			}
		} else if (length == ZERO3_10) {
			U32 zeroCount = 3 + reader.read_bits(3);
			for (U32 j = 0; j < zeroCount; j++) {
				codeLengths[codeIdx++] = 0;
			}
		} else if (length == ZERO11_138) {
			U32 zeroCount = 11 + reader.read_bits(7);
			for (U32 j = 0; j < zeroCount; j++) {
				codeLengths[codeIdx++] = 0;
			}
		} else {
			RUNTIME_ASSERT(false, "Bad length!");
		}
	}
}

void decompress_trees(BitReader& reader, HuffmanTree* outLitLen, HuffmanTree* outDist) {
	constexpr U32 maxHLit = 286;
	//constexpr U32 minHLit = 257;
	constexpr U32 maxHDist = 32;
	//constexpr U32 minHDistn = 1;
	constexpr U32 maxHCLen = 19;
	//constexpr U32 minHCLen = 4;

	U32 hlit = reader.read_bits(5) + 257;
	U32 hdist = reader.read_bits(5) + 1;
	U32 hclen = reader.read_bits(4) + 4;
	RUNTIME_ASSERT(hlit <= maxHLit, "HLIT out of range!");
	RUNTIME_ASSERT(hdist <= maxHDist, "HDIST out of range!");
	RUNTIME_ASSERT(hclen <= maxHCLen, "HCLEN out of range!");

	constexpr U32 codeLengthOrder[maxHCLen]{ 16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15 };
	U8 lengths[maxHCLen]{};
	for (U32 i = 0; i < hclen; i++) {
		lengths[codeLengthOrder[i]] = U8(reader.read_bits(3));
	}
	HuffmanTree decompressTree;
	decompressTree.build(lengths, maxHCLen);

	U8 litlenCodeLengths[maxHLit];
	read_tree_lengths(reader, decompressTree, hlit, litlenCodeLengths);

	U8 distCodeLengths[maxHDist];
	read_tree_lengths(reader, decompressTree, hdist, distCodeLengths);

	outLitLen->build(litlenCodeLengths, hlit);
	outDist->build(distCodeLengths, hdist);
}

Byte* inflate(MemoryArena& arena, Byte* data, Byte** result, U32* resultLength, U32 defaultBufferSize, HuffmanTree& litlenFixedTree, HuffmanTree& distFixedTree) {
	BitReader reader; reader.init(data);
	B32 finalBlock = false;
	U32 bufferSize = defaultBufferSize;
	U32 decompressedSize = 0;
	Byte* decompressedOutput = arena.alloc<Byte>(bufferSize);

	constexpr U32 extraBitLengthTable[29]{ 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0 };
	constexpr U32 startingLengthTable[29]{ 3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258 };
	constexpr U32 extraBitDistTable[30]{ 0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13 };
	constexpr U32 startingDistTable[30]{ 1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577 };
	constexpr U32 endOfBlock = 256;

	//U64 huffmanReadTime = 0;
	//U64 decompressTime = 0;
	//U64 totalReadTime = 0;
	while (!finalBlock) {
		finalBlock = reader.read_bits(1);

		DeflateCompressionType compressionType = static_cast<DeflateCompressionType>(reader.read_bits(2));
		RUNTIME_ASSERT(compressionType != DEFLATE_COMPRESSION_RESERVED, "Bad compression type! This value is reserved");
		if (compressionType == DEFLATE_COMPRESSION_NONE) {
			// Skip remaining bits in current processing byte
			reader.align_to_byte();
			// Read LEN and NLEN
			U16 len = reader.read_uint16();
			U16 nlen = reader.read_uint16();
			RUNTIME_ASSERT(len == (~nlen & 0xFFFF), "Data corruption? Length and the one's complement of length are not inverse of each other");
			Byte* storedData = reader.get_current_pointer();
			reader.increase_byte_pos(len);
			resize_buffer(arena, &decompressedOutput, &bufferSize, decompressedSize, decompressedSize + len);
			// Copy LEN bytes of data to output
			memcpy(decompressedOutput + decompressedSize, storedData, len);
			decompressedSize += len;
		} else if (compressionType == DEFLATE_COMPRESSION_FIXED || compressionType == DEFLATE_COMPRESSION_DYNAMIC) {
			HuffmanTree litLenTree;
			HuffmanTree distTree;
			HuffmanTree* litLenTreeRef = &litlenFixedTree;
			HuffmanTree* distTreeRef = &distFixedTree;
			//U64 time1 = __rdtsc();
			if (compressionType == DEFLATE_COMPRESSION_DYNAMIC) {
				decompress_trees(reader, &litLenTree, &distTree);
				litLenTreeRef = &litLenTree;
				distTreeRef = &distTree;
			}
			//decompressTime += __rdtsc() - time1;


			//U64 time2 = __rdtsc();
			while (true) {
				//U64 time = __rdtsc();
				U16 value = (*litLenTreeRef).read_next(reader);
				//huffmanReadTime += __rdtsc() - time;
				if (value == endOfBlock) {
					break;
				} else if (value < 256) {
					resize_buffer(arena, &decompressedOutput, &bufferSize, decompressedSize, decompressedSize + 1);
					decompressedOutput[decompressedSize++] = static_cast<U8>(value);
				} else if (value < 286) {
					value -= 257;
					U32 extraBits = extraBitLengthTable[value];
					U32 length = startingLengthTable[value] + reader.read_bits(extraBits);
					//time = __rdtsc();
					U32 distance = (*distTreeRef).read_next(reader);
					//huffmanReadTime += __rdtsc() - time;
					RUNTIME_ASSERT(distance < 30, "Distance read out of range");
					extraBits = extraBitDistTable[distance];
					distance = startingDistTable[distance] + reader.read_bits(extraBits);
					resize_buffer(arena, &decompressedOutput, &bufferSize, decompressedSize, decompressedSize + length);
					U32 start = decompressedSize - distance;
					for (U32 i = 0; i < length; i++) {
						decompressedOutput[decompressedSize + i] = decompressedOutput[start + i];
					}
					decompressedSize += length;
				} else {
					RUNTIME_ASSERT(false, "Read wrong value!");
				}
			}
			//totalReadTime += __rdtsc() - time2;
		}
	}

	//std::cout << "Huffman read time: " << huffmanReadTime << "\nDecompress time: " << decompressTime << "\nTotal read time: " << totalReadTime << "\n" << std::endl;

	*result = decompressedOutput;
	*resultLength = decompressedSize;

	reader.align_to_byte();
	return reader.get_current_pointer();
}

enum InterlaceMethod {
	NONE = 0,
	ADAM7 = 1
};

enum FilterMode {
	FILTER_MODE_NONE = 0,
	FILTER_MODE_SUB = 1,
	FILTER_MODE_UP = 2,
	FILTER_MODE_AVERAGE = 3,
	FILTER_MODE_PAETH = 4
};

struct ImageHeader {
	U32 width;
	U32 height;
	U8 bitDepth;
	B32 hasPalette;
	B32 hasColor;
	B32 hasAlpha;
	InterlaceMethod interlace;
};

void read_ihdr(ByteBuf& ihdr, ImageHeader& header) {
	header.width = ihdr.read_be32();
	header.height = ihdr.read_be32();
	header.bitDepth = ihdr.read_be8();

	U8 colorType = ihdr.read_be8();
	constexpr U8 allowedBitDepths[5]{ 1, 2, 4, 8, 16 };
	U8 bitCheckRange[2]{ 0, 5 };
	switch (colorType) {
	case 0: break;
	case 2: bitCheckRange[0] = 3; break;
	case 3: bitCheckRange[1] = 4; break;
	case 4: bitCheckRange[0] = 3; break;
	case 6: bitCheckRange[0] = 3; break;
	default: RUNTIME_ASSERT(false, "Color type is wrong!"); break;
	}
	B32 bitDepthVerified = false;
	for (U32 i = bitCheckRange[0]; i < bitCheckRange[1]; i++) {
		if (header.bitDepth == allowedBitDepths[i]) {
			bitDepthVerified = true;
			break;
		}
	}
	RUNTIME_ASSERT(bitDepthVerified, "Bit depth not acceptable for color type!");

	header.hasPalette = B32(colorType & 1);
	header.hasColor = B32(colorType & 2);
	header.hasAlpha = B32(colorType & 4);

	U8 compressionMethod = ihdr.read_be8();
	RUNTIME_ASSERT(compressionMethod == 0, "Compression method isn't deflate!");
	U8 filterMethod = ihdr.read_be8();
	RUNTIME_ASSERT(filterMethod == 0, "Filter method isn't recognized!");
	header.interlace = static_cast<InterlaceMethod>(ihdr.read_be8());
}

FINLINE U8 paeth_predictor(U8 left, U8 up, U8 upLeft) {
	I32 initialEstimate = left + up - upLeft;
	I32 distLeft = abs(initialEstimate - left);
	I32 distUp = abs(initialEstimate - up);
	I32 distUpLeft = abs(initialEstimate - upLeft);
	if (distLeft <= distUp && distLeft <= distUpLeft) {
		return left;
	} else if (distUp <= distUpLeft) {
		return up;
	} else {
		return upLeft;
	}
}

// Index 0 is the regular image format, transmitted line by line. Indices 1-8 represent the 7 interlaced passes
const U32 interlaceXOffset[8]{ 0, 0, 4, 0, 2, 0, 1, 0 };
const U32 interlaceYOffset[8]{ 0, 0, 0, 4, 0, 2, 0, 1 };
const U32 interlaceXStride[8]{ 1, 8, 8, 4, 4, 2, 2, 1 };
const U32 interlaceYStride[8]{ 1, 8, 8, 8, 4, 4, 2, 2 };

FINLINE Byte sample_line(Byte* line, U32 lineWidth, U32 x) {
	// Greater than or equals handles less than zero as well due to unsigned math
	if (line == nullptr || x >= lineWidth) {
		return 0;
	}
	return line[x];
}

Byte* translate_pass(ImageHeader& header, Byte* data, U32 dataSize, Byte* finalData, U32 pass, U32 numComponents, U32 bytesPerPixel, U32 pixelsPerByte) {
	U32 passWidth = (header.width - interlaceXOffset[pass] + (interlaceXStride[pass] - 1)) / interlaceXStride[pass];
	U32 passHeight = (header.height - interlaceYOffset[pass] + (interlaceYStride[pass] - 1)) / interlaceYStride[pass];
	U32 bytesPerLine = (passWidth * numComponents * header.bitDepth + 7) / 8;

	Byte* previousLine = nullptr;
	for (U32 y = 0; y < passHeight; y++) {
		FilterMode filterMode = static_cast<FilterMode>(*data++);
		RUNTIME_ASSERT(filterMode < 5, "Filter mode out of range");
		for (U32 xByte = 0; xByte < bytesPerLine; xByte++) {
			Byte currentByte = data[xByte];

			switch (filterMode) {
			case FILTER_MODE_NONE:
				break;
			case FILTER_MODE_SUB:
				currentByte = Byte(currentByte + sample_line(finalData, bytesPerLine, xByte - bytesPerPixel));
				break;
			case FILTER_MODE_UP:
				currentByte = Byte(currentByte + sample_line(previousLine, bytesPerLine, xByte));
				break;
			case FILTER_MODE_AVERAGE:
			{
				Byte left = sample_line(finalData, bytesPerLine, xByte - bytesPerPixel);
				Byte up = sample_line(previousLine, bytesPerLine, xByte);
				currentByte = Byte(currentByte + static_cast<U8>((left + up) / 2));
			}
			break;
			case FILTER_MODE_PAETH:
			{
				Byte left = sample_line(finalData, bytesPerLine, xByte - bytesPerPixel);
				Byte up = sample_line(previousLine, bytesPerLine, xByte);
				Byte upLeft = sample_line(previousLine, bytesPerLine, xByte - bytesPerPixel);
				currentByte = Byte(currentByte + paeth_predictor(left, up, upLeft));
			}
			break;
			}
			finalData[xByte] = currentByte;
		}
		data += bytesPerLine;
		previousLine = finalData;
		finalData += bytesPerLine;
	}
	return data;
}


void rescale_bit_depth(ImageHeader& header, Byte* finalData, RGBA8* finalImage, U32 pass, U32 numComponents, U32 bytesPerPixel, U32 pixelsPerByte, RGB8* palette, U32 paletteEntries, Byte* transparency, U32 transparencyEntries) {
	U32 passWidth = (header.width - interlaceXOffset[pass] + (interlaceXStride[pass] - 1)) / interlaceXStride[pass];
	U32 passHeight = (header.height - interlaceYOffset[pass] + (interlaceYStride[pass] - 1)) / interlaceYStride[pass];
	U32 paletteIndexMask = header.hasPalette ? 0x00000000 : 0xFFFFFFFF;
	U32 bytesPerLine = (passWidth * numComponents * header.bitDepth + 7) / 8;

	// Transparency but not null;
	Byte transparencyDummyData[8];
	Byte* checkTransparency = transparency ? transparency : transparencyDummyData;
	for (U32 y = 0; y < passHeight; y++) {
		U32 finalY = y * interlaceYStride[pass] + interlaceYOffset[pass];
		//U32 lineOffset = (y * bytesPerPixel * passWidth + pixelsPerByte-1) / pixelsPerByte;
		Byte* line = finalData + bytesPerLine * y;
		for (U32 x = 0; x < passWidth; x++) {
			U32 finalX = x * interlaceXStride[pass] + interlaceXOffset[pass];

			U8 rescaledComponents[4];
			B32 transparencyCheck = true;
			// Read up to 4 components into the array
			for (U32 i = 0; i < numComponents; i++) {
				U32 element = (x * numComponents + i);

				switch (header.bitDepth) {
				case 16: {
					// Only use most significant byte
					rescaledComponents[i] = line[element * 2];
					transparencyCheck &= line[element * 2] == checkTransparency[i * 2] && line[element * 2 + 1] == checkTransparency[i * 2 + 1];
				} break;
				case 8: {
					// pass through
					rescaledComponents[i] = line[element];
					transparencyCheck &= line[element] == checkTransparency[i];
				} break;
				case 4: {
					rescaledComponents[i] = U8((line[element / 2] >> ((1 - (element & 1)) * 4)) & 15);
					transparencyCheck &= rescaledComponents[i] == checkTransparency[i];
					// Put the bits in both bottom and top half of byte, that way 0 maps to 0 and 15 maps to 255.
					rescaledComponents[i] |= (rescaledComponents[i] << 4) & paletteIndexMask;
				} break;
				case 2: {
					rescaledComponents[i] = U8((line[element / 4] >> ((3 - (element & 3)) * 2)) & 3);
					transparencyCheck &= rescaledComponents[i] == checkTransparency[i];
					// Repeat the 2 bits 4 times in the byte, same reason as above
					rescaledComponents[i] |= (rescaledComponents[i] << 2) & paletteIndexMask;
					rescaledComponents[i] |= (rescaledComponents[i] << 4) & paletteIndexMask;
				} break;
				case 1: {
					// Extract the bit and choose 255 or 0.
					rescaledComponents[i] = U8(((line[element / 8] >> (7 - (element & 7))) & 1) * (1 + 254 * (paletteIndexMask > 0)));
					transparencyCheck &= (rescaledComponents[i] > 0) == checkTransparency[i];
				} break;
				}
			}
			// Map those components to the RGBA final output.
			U32 finalidx = finalY * header.width + finalX;
			if (header.hasPalette) {
				U8 index = rescaledComponents[0];
				RUNTIME_ASSERT(index < paletteEntries, "Palette index was out of range");
				RGB8 color = palette[index];
				U8 alpha;
				if (index < transparencyEntries) {
					alpha = transparency[index];
				} else {
					alpha = 255;
				}
				finalImage[finalidx] = RGBA8{ color.r, color.g, color.b, alpha };
			} else {
				U32 numComponentsWithExtraAlpha = numComponents + (transparency != nullptr);
				if (transparency) {
					rescaledComponents[numComponentsWithExtraAlpha - 1] = U8((!!transparencyCheck) * 255);
				}
				switch (numComponentsWithExtraAlpha) {
				case 1:
					// One greyscale component
					finalImage[finalidx] = RGBA8{ rescaledComponents[0], rescaledComponents[0], rescaledComponents[0], 255 };
					break;
				case 2:
					// Greyscale with alpha
					finalImage[finalidx] = RGBA8{ rescaledComponents[0], rescaledComponents[0], rescaledComponents[0], rescaledComponents[1] };
					break;
				case 3:
					// RGB color only
					finalImage[finalidx] = RGBA8{ rescaledComponents[0], rescaledComponents[1], rescaledComponents[2], 255 };
					break;
				case 4:
					// RGBA
					finalImage[finalidx] = RGBA8{ rescaledComponents[0], rescaledComponents[1], rescaledComponents[2], rescaledComponents[3] };
					break;
				}
			}

		}
	}
}

// Fast path method for when the bit depth is the same;
template<typename OriginalFormat>
void rescale_components(ImageHeader& header, OriginalFormat* finalData, RGBA8* finalImage, RGB8* palette, U32 paletteEntries, Byte* transparency, U32 transparencyEntries) {
	U32 pixelCount = header.width * header.height;
	if (header.hasPalette && transparencyEntries > 0) {
		for (U32 pixel = 0; pixel < pixelCount; pixel++) {
			OriginalFormat index = finalData[pixel];
			finalImage[pixel] = palette[index.r].to_rgba8();
			if (index.r < transparencyEntries) {
				finalImage[pixel].a = transparency[index.r];
			}
		}
	} else if (header.hasPalette) {
		for (U32 pixel = 0; pixel < pixelCount; pixel++) {
			OriginalFormat index = finalData[pixel];
			finalImage[pixel] = palette[index.r].to_rgba8();
		}
	} else if (transparency) {
		for (U32 pixel = 0; pixel < pixelCount; pixel++) {
			finalImage[pixel] = finalData[pixel].to_rgba8();
			if (finalData[pixel] == *reinterpret_cast<OriginalFormat*>(transparency)) {
				finalImage[pixel].a = 0;
			}
		}
	} else {
		for (U32 pixel = 0; pixel < pixelCount; pixel++) {
			finalImage[pixel] = finalData[pixel].to_rgba8();
		}
	}
}

void translate_png_data(ImageHeader& header, Byte* data, U32 dataSize, RGBA8* finalImage, RGB8* palette, U32 paletteEntries, Byte* transparency, U32 transparencyEntries) {
	U32 numComponents = header.hasColor ? 3u : 1u;
	if (header.hasAlpha) {
		numComponents += 1;
	}
	if (header.hasPalette) {
		// If it has a palette, each pixel is an index
		numComponents = 1;
	}
	U32 bitsPerPixel = header.bitDepth * numComponents;
	U32 bytesPerPixel = (bitsPerPixel + 7) / 8;
	U32 pixelsPerByte = max(8 / (header.bitDepth * numComponents), 1u);

	MemoryArena& arena = get_scratch_arena();
	U64 oldArenaStackPtr = arena.stackPtr;

	Byte* finalData;
	if (header.bitDepth != 8 || header.interlace || numComponents != 4) {
		finalData = arena.alloc<Byte>(((header.width * bitsPerPixel + 7) / 8) * header.height);
	} else {
		finalData = reinterpret_cast<Byte*>(finalImage);
	}

	if (header.interlace) {
		for (U32 pass = 1; pass <= 7; pass++) {
			if (header.width <= interlaceXOffset[pass] || header.height <= interlaceYOffset[pass]) {
				continue;
			}
			data = translate_pass(header, data, dataSize, finalData, pass, numComponents, bytesPerPixel, pixelsPerByte);
			rescale_bit_depth(header, finalData, finalImage, pass, numComponents, bytesPerPixel, pixelsPerByte, palette, paletteEntries, transparency, transparencyEntries);
		}
	} else {
		data = translate_pass(header, data, dataSize, finalData, 0, numComponents, bytesPerPixel, pixelsPerByte);
		if (header.bitDepth != 8) {
			rescale_bit_depth(header, finalData, finalImage, 0, numComponents, bytesPerPixel, pixelsPerByte, palette, paletteEntries, transparency, transparencyEntries);
		} else if (numComponents != 4) {
			// Fast path options
			if (numComponents == 3) {
				rescale_components<RGB8>(header, reinterpret_cast<RGB8*>(finalData), finalImage, palette, paletteEntries, transparency, transparencyEntries);
			} else if (numComponents == 2) {
				rescale_components<RG8>(header, reinterpret_cast<RG8*>(finalData), finalImage, palette, paletteEntries, transparency, transparencyEntries);
			} else if (numComponents == 1) {
				rescale_components<R8>(header, reinterpret_cast<R8*>(finalData), finalImage, palette, paletteEntries, transparency, transparencyEntries);
			}
		}
	}

	arena.stackPtr = oldArenaStackPtr;
}

HuffmanTree litlenFixedTree;
HuffmanTree distFixedTree;

// There's a lot that should be done to improve this utility. For one, it's pretty slow. For another, error handling sucks. Error codes should be returned.
void read_image(MemoryArena& arena, RGBA8** outImage, U32* outWidth, U32* outHeight, StrA fileName) {
	*outImage = nullptr;
	*outWidth = 0;
	*outHeight = 0;
	MemoryArena& stackArena = get_scratch_arena_excluding(arena);
	U64 oldArenaStackPtr = stackArena.stackPtr;
	//U64 readToBytesTime = __rdtsc();
	U32 fileSize;
	U8* file = read_full_file_to_arena<U8>(&fileSize, stackArena, fileName);
	//readToBytesTime = __rdtsc() - readToBytesTime;
	ByteBuf data; data.wrap(file, fileSize);

	Byte pngSignature[]{ 0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A };
	if (file && data.capacity > 8 && memcmp(data.bytes, pngSignature, 8) == 0) {
		data.offset += 8;

		//U64 time1 = __rdtsc();

		// Blocks we care about
		ByteBuf ihdr{};
		U32 idatSize = 0;
		void* idatAllocation = nullptr;
		ByteBuf idat{};
		U32 paletteSize = 0;
		ByteBuf plte{};
		U32 transparencySize = 0;
		ByteBuf trns{};
		// Load chunks
		while (data.has_data_left(sizeof(U32) + sizeof(U32))) {
			U32 length = data.read_be32();
			if (memcmp(data.bytes + data.offset, "IHDR", 4) == 0) {
				RUNTIME_ASSERT(idat.bytes == nullptr, "IDAT appeared before IHDR!");
				RUNTIME_ASSERT(plte.bytes == nullptr, "PLTE appeared before IHDR!");
				RUNTIME_ASSERT(trns.bytes == nullptr, "tRNS appeared before IHDR!");
				ihdr.wrap(data.bytes + data.offset + 4, data.capacity - data.offset - 4);
			} else if (memcmp(data.bytes + data.offset, "IDAT", 4) == 0) {
				if (idat.bytes) {
					// More than one IDAT chunk, put them all in the same memory. I could make some system that uses less memory and jumps between idat chunks, but this simplifies things a bit.
					if (idatAllocation) {
						idatAllocation = stackArena.realloc<Byte>(reinterpret_cast<Byte*>(idatAllocation), idatSize, idatSize + length);
						idat.wrap(idatAllocation, idatSize + length);
						memcpy(idat.bytes + idatSize, data.bytes + data.offset + 4, length);
					} else {
						Byte* newData = stackArena.alloc<Byte>(idatSize + length);
						memcpy(newData, idat.bytes, idatSize);
						memcpy(newData + idatSize, data.bytes + data.offset + 4, length);

						idatAllocation = newData;
						idat.wrap(idatAllocation, idatSize + length);
					}
				} else {
					idat.wrap(data.bytes + data.offset + 4, data.capacity - data.offset - 4);
				}
				idatSize += length;
			} else if (memcmp(data.bytes + data.offset, "tRNS", 4) == 0) {
				RUNTIME_ASSERT(idat.bytes == nullptr, "IDAT appeared before tRNS!");
				trns.wrap(data.bytes + data.offset + 4, data.capacity - data.offset - 4);
				transparencySize = length;
			} else if (memcmp(data.bytes + data.offset, "PLTE", 4) == 0) {
				RUNTIME_ASSERT(trns.bytes == nullptr, "tRNS appeared before PLTE!");
				RUNTIME_ASSERT((length % 3) == 0, "Pallet length not given in RGB triplets!");
				paletteSize = length / 3;
				RUNTIME_ASSERT(paletteSize <= 256, "Palette length is greater than max allowed size of 256!");
				plte.wrap(data.bytes + data.offset + 4, data.capacity - data.offset - 4);
			} else if (memcmp(data.bytes + data.offset, "IEND", 4) == 0) {
				break;
			} else {
				// Skip this block, we don't care about it
				// block name
				data.skip(4);
				// main block
				data.skip(length);
				// crc32
				data.skip(4);
				continue;
			}
			U32 blockCrc = crc32(data.bytes + data.offset, length + 4);
			// chunk name
			data.skip(4);
			// chunk data
			data.skip(length);
			U32 checkCrc = data.read_be32();
			RUNTIME_ASSERT(blockCrc == checkCrc, "Block CRC32 doesn't match! Data corruption?");
		}
		RUNTIME_ASSERT(ihdr.bytes, "No header!");
		RUNTIME_ASSERT(idat.bytes, "No data!");

		ImageHeader header;
		read_ihdr(ihdr, header);

		if (header.hasPalette) {
			RUNTIME_ASSERT(plte.bytes, "Palette required in header, but none provided!");
			RUNTIME_ASSERT(transparencySize <= paletteSize, "tRNS has more entries than PLTE!");
		} else if (trns.bytes) {
			if (header.hasColor) {
				RUNTIME_ASSERT(transparencySize == 6, "Transparency not a 6 byte RGB triplet!");
			} else {
				RUNTIME_ASSERT(transparencySize == 2, "Transparency not a 2 byte single value!");
			}
		}


		U8 cmf = idat.read_be8();
		CompressionMethod compression = static_cast<CompressionMethod>(cmf & 0b1111u);
		RUNTIME_ASSERT(compression == COMPRESSION_METHOD_DEFLATE, "Compression wasn't deflate, this is not supported");
		U32 windowBits = ((cmf >> 4) & 0b1111u) + 8;
		RUNTIME_ASSERT(windowBits <= 15, "Window bits was greater than 15! This is illegal");

		U8 flg = idat.read_be8();
		B32 dictPresent = (flg >> 5u) & 1u;
		RUNTIME_ASSERT(dictPresent == false, "Unknown dictionary!");
		//CompressionLevel compressionLevel = static_cast<CompressionLevel>((flg >> 6u) & 0b11u);


		Byte* decompressedData;
		U32 decompressedSize;
		U32 numComponents = 1u + (!!header.hasColor) * 2u + (!!header.hasAlpha);
		U32 sizeGuess = ((header.width * numComponents * header.bitDepth + 7) / 8 + 1) * header.height;
		//U64 inflateTime = __rdtsc();
		Byte* newIdatPtr = inflate(stackArena, idat.bytes + idat.offset, &decompressedData, &decompressedSize, sizeGuess, litlenFixedTree, distFixedTree);
		idat.skip(U32(newIdatPtr - (idat.bytes + idat.offset)));
		//inflateTime = __rdtsc() - inflateTime;
		U32 storedAdler32 = idat.read_be32();
		//U64 adlerTime = __rdtsc();
		U32 currentAdler32 = adler32(decompressedData, decompressedSize);
		//adlerTime = __rdtsc() - adlerTime;
		RUNTIME_ASSERT(storedAdler32 == currentAdler32, "Data checksums don't match!");
		if (currentAdler32 != storedAdler32) {
			RUNTIME_ASSERT(false, "Inflate failed!");
		}

		RGBA8* finalImage = arena.alloc<RGBA8>(header.width * header.height);
		//U64 translationTime = __rdtsc();
		translate_png_data(header, decompressedData, decompressedSize, finalImage, reinterpret_cast<RGB8*>(plte.bytes), paletteSize, trns.bytes, transparencySize);
		//translationTime = __rdtsc() - translationTime;

		//std::cout << "Translation time: " << translationTime << std::endl;
		//std::cout << "Other time: " << time1 << std::endl;
		//std::cout << "Inflate time: " << inflateTime << std::endl;
		//std::cout << "Read to bytes time: " << readToBytesTime << std::endl;
		//std::cout << "Adler time: " << adlerTime << std::endl;
		//std::cout << "Inflate time: " << inflateTime << "\nPuff time:" << puffTime << "\nTranslate time: " << translationTime << "\n" << std::endl;

		*outImage = finalImage;
		*outWidth = header.width;
		*outHeight = header.height;
	}
	stackArena.stackPtr = oldArenaStackPtr;
}

B32 write_image(StrA fileName, RGBA8* pixels, U32 width, U32 height) {
	MemoryArena& stackArena = get_scratch_arena();
	U64 oldArenaStackPtr = stackArena.stackPtr;
	U32 rowByteWidth = width * 4 + 1;
	U32 imageDataSize = rowByteWidth * height;
	Byte* imgData = stackArena.alloc<Byte>(imageDataSize);
	for (U32 i = 0; i < height; i++) {
		imgData[i * rowByteWidth] = 0;
		memcpy(&imgData[i * rowByteWidth + 1], &pixels[i * width], width * 4);
	}
	U32 numBlocks = (imageDataSize + U16_MAX - 1) / U16_MAX;
	U32 idatSize = (2 + (1 + 2 + 2) * numBlocks + imageDataSize + 4);
	// Grouped by chunk
	U32 fileSize = (8) + (8 + (13) + 4) + (8 + idatSize + 4) + (8 + 4);

	Byte* fileData = stackArena.alloc<Byte>(fileSize);

	ByteBuf writer; writer.wrap(fileData, fileSize);

	Byte pngSignature[]{ 0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A };
	writer.write_bytes(pngSignature, 8);

	// Write header (13 bytes long)
	writer.write_be32(13);
	writer.write_bytes("IHDR", 4);
	writer.write_be32(width);
	writer.write_be32(height);
	// Bit depth
	writer.write_be8(8);
	// Mode 6, alpha, color, no palette
	writer.write_be8(6);
	// Compression method (deflate)
	writer.write_be8(0);
	// Regular filtering
	writer.write_be8(0);
	// No interlacing
	writer.write_be8(0);
	writer.write_be32(crc32(writer.bytes + writer.offset - 13 - 4, 13 + 4));

	// Write data
	writer.write_be32(idatSize);
	writer.write_bytes("IDAT", 4);
	// cmf
	writer.write_be8(8);
	// flg (cmf * 256 + flg must be a multiple of 31)
	writer.write_be8(29);
	// Just write a bunch of store blocks for now. Might do encoding later
	U32 size = idatSize - 6;
	Byte* currentImgPtr = imgData;
	while (size > 0) {
		size -= 5;
		U16 len = U16(min(0xFFFFu, size));
		// Compression type store
		writer.write_be8(((size - len) > 0) ? 0u : 1u);
		writer.write_be16(bswap16(len));
		writer.write_be16(bswap16(U16(~len)));
		writer.write_bytes(currentImgPtr, len);
		currentImgPtr += len;
		size -= len;
	}
	writer.write_be32(adler32(imgData, imageDataSize));
	writer.write_be32(crc32(writer.bytes + writer.offset - idatSize - 4, idatSize + 4));

	// Write end
	writer.write_be32(0);
	writer.write_bytes("IEND", 4);
	writer.write_be32(crc32(writer.bytes + writer.offset - 4, 4));

	B32 fileWriteSuccess = write_data_to_file(fileName, fileData, fileSize);
	stackArena.stackPtr = oldArenaStackPtr;
	return fileWriteSuccess;
}

void init_loader() {
	compute_crc_table();
	generate_fixed_tree(litlenFixedTree, distFixedTree);
}

}