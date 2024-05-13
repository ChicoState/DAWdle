#pragma once
#include "DrillLib.h"

namespace FFT {

F32 FFT_COS_TABLE[]{
#include "FFTCosineTable.txt"
};

template<B32 inverse, B32 hasInputY>
void fft_1024(F32* outX, F32* outY, F32* inX, F32* inY) {
	__m256 zero = _mm256_setzero_ps();
	__m256i bitReverseLookup = _mm256_setr_epi8(0b000000, 0b100000, 0b010000, 0b110000, 0b001000, 0b101000, 0b011000, 0b111000, 0b000100, 0b100100, 0b010100, 0b110100, 0b001100, 0b101100, 0b011100, 0b111100, 0b000000, 0b100000, 0b010000, 0b110000, 0b001000, 0b101000, 0b011000, 0b111000, 0b000100, 0b100100, 0b010100, 0b110100, 0b001100, 0b101100, 0b011100, 0b111100);
	__m256i lowBitMask = _mm256_set1_epi32(0x0F0F0F0F);
	__m256i tenBitMask = _mm256_set1_epi32((1 << 10) - 1);
	__m256i normalIndices = _mm256_setr_epi32(0, 1, 2, 3, 4, 5, 6, 7);
	__m256i eight = _mm256_set1_epi32(8);

	__m256 rotation1, rotation2, rotation3, rotation4;
	if constexpr (inverse) {
		rotation1 = _mm256_setr_ps(1.0F, 0.0F, 1.0F, 0.0F, 1.0F, 0.0F, 1.0F, 0.0F);
		rotation2 = _mm256_setr_ps(0.0F, 1.0F, 0.0F, 1.0F, 0.0F, 1.0F, 0.0F, 1.0F);
		rotation3 = _mm256_setr_ps(1.0F, 0.707107F, 0.0F, -0.707107F, 1.0F, 0.707107F, 0.0F, -0.707107F);
		rotation4 = _mm256_setr_ps(0.0F, 0.707107F, 1.0F, 0.707107F, 0.0F, 0.707107F, 1.0F, 0.707107F);
	} else {
		rotation1 = _mm256_setr_ps(1.0F, 0.0F, 1.0F, 0.0F, 1.0F, 0.0F, 1.0F, 0.0F);
		rotation2 = _mm256_setr_ps(0.0F, -1.0F, 0.0F, -1.0F, 0.0F, -1.0F, 0.0F, -1.0F);
		rotation3 = _mm256_setr_ps(1.0F, 0.707107F, 0.0F, -0.707107F, 1.0F, 0.707107F, 0.0F, -0.707107F);
		rotation4 = _mm256_setr_ps(0.0F, -0.707107F, -1.0F, -0.707107F, 0.0F, -0.707107F, -1.0F, -0.707107F);
	}

	for (U32 i = 0; i < 1024; i += 8) {
		// x represents a hard 0 bit, number represents source bit index
		// nybbles xxxx|xx01|2345|6789 -> xx10|xxxx|xx98|76xx
		__m256i permutedIndexBits1 = _mm256_shuffle_epi8(bitReverseLookup, _mm256_and_si256(normalIndices, lowBitMask));
		// nybbles xxxx|xx01|2345|6789 -> xxxx|xxxx|xx54|32xx
		__m256i permutedIndexBits2 = _mm256_shuffle_epi8(bitReverseLookup, _mm256_srli_epi16(normalIndices, 4));
		// (xx10xxxxxx9876xx << 4 & tenBitMask) | xxxxxxxxxx5432xx | xx10xxxxxx9876xx >> 12 -> xxxxxx9876543210
		__m256i permutedIndices = _mm256_or_si256(_mm256_or_si256(
			_mm256_and_si256(_mm256_slli_epi32(permutedIndexBits1, 4), tenBitMask),
			permutedIndexBits2),
			_mm256_srli_epi32(permutedIndexBits1, 12));

		__m256 resultX = _mm256_i32gather_ps(inX, permutedIndices, 4);
		__m256 resultY = zero;
		if constexpr (hasInputY) {
			// If this isn't in an if constexpr, the compiler can't optimize out a lot of the operations below, resulting in significantly worse performance
			resultY = _mm256_i32gather_ps(inY, permutedIndices, 4);
		}

		// Constants:
		// 1,0
		// 
		// 1,0
		// 0,-1
		// 
		// 1,0
		// 0.707107,-0.707107
		// 0,-1
		// -0.707107,-0.707107
		// 
		// outx1 = evenX + (oddX *  c1 + oddY * c2)
		// outy1 = evenY + (oddX * -c2 + oddY * c1)
		// outx2 = evenX - (oddX *  c1 + oddY * c2)
		// outy2 = evenY - (oddX * -c2 + oddY * c1)

		__m256 evenX = resultX;
		__m256 oddX = _mm256_castsi256_ps(_mm256_srli_si256(_mm256_castps_si256(resultX), 4));
		__m256 evenY = resultY;
		__m256 oddY = _mm256_castsi256_ps(_mm256_srli_si256(_mm256_castps_si256(resultY), 4));
		__m256 outX1 = _mm256_add_ps(evenX, oddX);
		__m256 outY1 = _mm256_add_ps(evenY, oddY);
		__m256 outX2 = _mm256_sub_ps(evenX, oddX);
		__m256 outY2 = _mm256_sub_ps(evenY, oddY);
		resultX = _mm256_blend_ps(outX1, _mm256_castsi256_ps(_mm256_slli_si256(_mm256_castps_si256(outX2), 4)), 0b10101010);
		resultY = _mm256_blend_ps(outY1, _mm256_castsi256_ps(_mm256_slli_si256(_mm256_castps_si256(outY2), 4)), 0b10101010);

		evenX = resultX;
		oddX = _mm256_castsi256_ps(_mm256_srli_si256(_mm256_castps_si256(resultX), 8));
		evenY = resultY;
		oddY = _mm256_castsi256_ps(_mm256_srli_si256(_mm256_castps_si256(resultY), 8));
		outX1 = _mm256_fmadd_ps(oddX, rotation1, _mm256_fmadd_ps(oddY, rotation2, evenX));
		outY1 = _mm256_fnmadd_ps(oddX, rotation2, _mm256_fmadd_ps(oddY, rotation1, evenY));
		outX2 = _mm256_fnmadd_ps(oddX, rotation1, _mm256_fnmadd_ps(oddY, rotation2, evenX));
		outY2 = _mm256_fmadd_ps(oddX, rotation2, _mm256_fnmadd_ps(oddY, rotation1, evenY));
		resultX = _mm256_blend_ps(outX1, _mm256_castsi256_ps(_mm256_slli_si256(_mm256_castps_si256(outX2), 8)), 0b11001100);
		resultY = _mm256_blend_ps(outY1, _mm256_castsi256_ps(_mm256_slli_si256(_mm256_castps_si256(outY2), 8)), 0b11001100);

		evenX = resultX;
		oddX = _mm256_castps128_ps256(_mm256_extractf128_ps(resultX, 1));
		evenY = resultY;
		oddY = _mm256_castps128_ps256(_mm256_extractf128_ps(resultY, 1));
		outX1 = _mm256_fmadd_ps(oddX, rotation3, _mm256_fmadd_ps(oddY, rotation4, evenX));
		outY1 = _mm256_fnmadd_ps(oddX, rotation4, _mm256_fmadd_ps(oddY, rotation3, evenY));
		outX2 = _mm256_fnmadd_ps(oddX, rotation3, _mm256_fnmadd_ps(oddY, rotation4, evenX));
		outY2 = _mm256_fmadd_ps(oddX, rotation4, _mm256_fnmadd_ps(oddY, rotation3, evenY));
		resultX = _mm256_insertf128_ps(outX1, _mm256_castps256_ps128(outX2), 1);
		resultY = _mm256_insertf128_ps(outY1, _mm256_castps256_ps128(outY2), 1);

		_mm256_storeu_ps(outX + i, resultX);
		_mm256_storeu_ps(outY + i, resultY);
		normalIndices = _mm256_add_epi32(normalIndices, eight);
	}

	F32* table = FFT_COS_TABLE + 10; // Skip first three iteration entries
	__m256 two = _mm256_set1_ps(2.0F);
	for (U32 iteration = 3; iteration < 10; iteration++) {
		U32 stride = 1 << iteration;
		U32 halfStride = stride >> 1;
		U32 iterationMask = stride - 1;
		for (U32 i = 0; i < 512; i += 8) {
			U32 index = ((i & ~iterationMask) << 1) + (i & iterationMask);
			// This algorithm is mostly loads and stores. Switching to a radix 4 (or 2^2) FFT would help with that, but I don't have time right now
			__m256 rotX = _mm256_loadu_ps(table + (i & iterationMask));
			__m256 rotY = _mm256_loadu_ps(table + (i & iterationMask) + halfStride);
			__m256 evenX = _mm256_loadu_ps(outX + index);
			__m256 evenY = _mm256_loadu_ps(outY + index);
			__m256 oddX = _mm256_loadu_ps(outX + index + stride);
			__m256 oddY = _mm256_loadu_ps(outY + index + stride);
			__m256 resultX1, resultY1, resultX2, resultY2;
			if constexpr (inverse) {
				resultX1 = _mm256_fmadd_ps(oddX, rotX, _mm256_fnmadd_ps(oddY, rotY, evenX));
				resultY1 = _mm256_fmadd_ps(oddX, rotY, _mm256_fmadd_ps(oddY, rotX, evenY));
				resultX2 = _mm256_fmsub_ps(evenX, two, resultX1);
				resultY2 = _mm256_fmsub_ps(evenY, two, resultY1);
			} else {
				resultX1 = _mm256_fmadd_ps(oddX, rotX, _mm256_fmadd_ps(oddY, rotY, evenX));
				resultY1 = _mm256_fnmadd_ps(oddX, rotY, _mm256_fmadd_ps(oddY, rotX, evenY));
				resultX2 = _mm256_fmsub_ps(evenX, two, resultX1);
				resultY2 = _mm256_fmsub_ps(evenY, two, resultY1);
			}
			_mm256_storeu_ps(outX + index, resultX1);
			_mm256_storeu_ps(outY + index, resultY1);
			_mm256_storeu_ps(outX + index + stride, resultX2);
			_mm256_storeu_ps(outY + index + stride, resultY2);
		}
		table += stride + halfStride;
	}
}

}