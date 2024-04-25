#pragma once

#include "DrillLib.h"
#include "ParseTools.h"
#include "PNG.h"

namespace MSDFGenerator {

struct ShapeSegmentHeader;

struct SignedDistance {
	const ShapeSegmentHeader* segment;
	F32 t;
	F32 distance;
	F32 orthogonality;

	B32 operator>(const SignedDistance& other) {
		if (distance == other.distance) {
			return orthogonality < other.orthogonality;
		} else {
			return distance > other.distance;
		}
	}

	B32 operator<(const SignedDistance& other) {
		if (distance == other.distance) {
			// The orthogonality metric will screw up with a sharp vertex where a line or curve meets a curve with the same direction at that point, but I can't be bothered to actually fix this
			// Mone of my fonts have shapes like that anyway
			return orthogonality > other.orthogonality;
		} else {
			return distance < other.distance;
		}
	}

	SignedDistance abs() {
		SignedDistance dist{ *this };
		dist.distance = absf32(distance);
		return dist;
	}
};

template<typename T>
F32 bezier_pseudo_distance(const T& producer, V2F32 pos, F32 t);
template<typename T>
SignedDistance numerical_distance_solve(V2F32 pos, const T& producer, F32 eps);

enum ShapeSegmentType {
	SHAPE_SEGMENT_TYPE_LINEAR,
	SHAPE_SEGMENT_TYPE_QUADRATIC,
	SHAPE_SEGMENT_TYPE_CUBIC,
	SHAPE_SEGMENT_TYPE_Count
};
struct ShapeSegmentHeader {
	ShapeSegmentType type;
	ShapeSegmentHeader* next;
	ShapeSegmentHeader* prev;
	RGBA8 color[2];

	RGBA8 color_at(F32 t) const {
		return t > 0.5F ? color[1] : color[0];
	}
};
struct ShapeSegmentLinear {
	ShapeSegmentHeader header;
	V2F32 positions[2];

	V2F32 eval(F32 t) const {
		// These checks are necessary, point evaulation at vertices must be exact
		return t <= 0.0F ? positions[0] :
			t >= 1.0F ? positions[1] :
			mix(positions[0], positions[1], t);
	}
	V2F32 direction(F32 t) const {
		return normalize(positions[1] - positions[0]);
	}
	SignedDistance distance(V2F32 pos) const {
		F32 t = time_along_line(pos, positions[0], positions[1]);
		V2F32 pointOnLine = t <= 0.0F ? positions[0] :
			t >= 1.0F ? positions[1] :
			mix(positions[0], positions[1], t);
		F32 dist = ::distance(pos, pointOnLine);
		F32 crossZ = cross(normalize(positions[1] - positions[0]), normalize(pos - pointOnLine));
		dist *= F32(signumf32(crossZ));
		return SignedDistance{ &header, t, dist, absf32(crossZ) };
	}
	F32 pseudo_distance(V2F32 pos, F32 t) const {
		V2F32 pointOnLine = mix(positions[0], positions[1], t);
		F32 dist = length(pos - pointOnLine);
		F32 side = F32(signumf32(cross(normalize(positions[1] - positions[0]), normalize(pos - pointOnLine))));
		return dist * side;
	}
};
struct ShapeSegmentQuadratic {
	ShapeSegmentHeader header;
	V2F32 positions[3];

	V2F32 eval(F32 t) const {
		// Positions at vertices must be exact
		if (t <= 0.0F) {
			return positions[0];
		} else if (t >= 0.0F) {
			return positions[2];
		}
		V2F32 p1 = mix(positions[0], positions[1], t);
		V2F32 p2 = mix(positions[1], positions[2], t);
		return mix(p1, p2, t);
	}
	V2F32 direction(F32 t) const {
		V2F32 p1 = mix(positions[0], positions[1], t);
		V2F32 p2 = mix(positions[1], positions[2], t);
		return normalize(p2 - p1);
	}
	SignedDistance distance(V2F32 pos) const {
		return numerical_distance_solve(pos, *this, 0.001F);
	}
	F32 pseudo_distance(V2F32 pos, F32 t) const {
		return bezier_pseudo_distance(*this, pos, t);
	}
};
struct ShapeSegmentCubic {
	ShapeSegmentHeader header;
	V2F32 positions[4];

	V2F32 eval(F32 t) const {
		// Positions at vertices must be exact
		if (t <= 0.0F) {
			return positions[0];
		} else if (t >= 1.0F) {
			return positions[3];
		}
		V2F32 pA1 = mix(positions[0], positions[1], t);
		V2F32 pA2 = mix(positions[1], positions[2], t);
		V2F32 pA3 = mix(positions[2], positions[3], t);
		V2F32 pB1 = mix(pA1, pA2, t);
		V2F32 pB2 = mix(pA2, pA3, t);
		return mix(pB1, pB2, t);
	}
	V2F32 direction(F32 t) const {
		// Annoying degenerate case produced by inkscape
		if (t < 0.0001F && epsilon_eq(positions[0], positions[1], 0.0001F)) {
			return normalize(positions[2] - positions[0]);
		} else if (t > 0.9999F && epsilon_eq(positions[2], positions[3], 0.0001F)) {
			return normalize(positions[3] - positions[1]);
		}

		V2F32 pA1 = mix(positions[0], positions[1], t);
		V2F32 pA2 = mix(positions[1], positions[2], t);
		V2F32 pA3 = mix(positions[2], positions[3], t);
		V2F32 pB1 = mix(pA1, pA2, t);
		V2F32 pB2 = mix(pA2, pA3, t);
		return normalize(pB2 - pB1);
	}
	SignedDistance distance(V2F32 pos) const {
		return numerical_distance_solve<ShapeSegmentCubic>(pos, *this, 0.001F);
	}
	F32 pseudo_distance(V2F32 pos, F32 t) const {
		return bezier_pseudo_distance(*this, pos, t);
	}
};
union ShapeSegment {
	ShapeSegmentHeader header;
	ShapeSegmentLinear linear;
	ShapeSegmentQuadratic quadratic;
	ShapeSegmentCubic cubic;
};
V2F32 shape_segment_direction(const ShapeSegmentHeader* segment, F32 t) {
	switch (segment->type) {
	case SHAPE_SEGMENT_TYPE_LINEAR: return reinterpret_cast<const ShapeSegmentLinear*>(segment)->direction(t);
	case SHAPE_SEGMENT_TYPE_QUADRATIC: return reinterpret_cast<const ShapeSegmentQuadratic*>(segment)->direction(t);
	case SHAPE_SEGMENT_TYPE_CUBIC: return reinterpret_cast<const ShapeSegmentCubic*>(segment)->direction(t);
	default: return V2F32{};
	}
}
V2F32 shape_segment_eval(const ShapeSegmentHeader* segment, F32 t) {
	switch (segment->type) {
	case SHAPE_SEGMENT_TYPE_LINEAR: return reinterpret_cast<const ShapeSegmentLinear*>(segment)->eval(t);
	case SHAPE_SEGMENT_TYPE_QUADRATIC: return reinterpret_cast<const ShapeSegmentQuadratic*>(segment)->eval(t);
	case SHAPE_SEGMENT_TYPE_CUBIC: return reinterpret_cast<const ShapeSegmentCubic*>(segment)->eval(t);
	default: return V2F32{};
	}
}
SignedDistance shape_segment_distance(const ShapeSegmentHeader* segment, V2F32 pos) {
	switch (segment->type) {
	case SHAPE_SEGMENT_TYPE_LINEAR: return reinterpret_cast<const ShapeSegmentLinear*>(segment)->distance(pos);
	case SHAPE_SEGMENT_TYPE_QUADRATIC: return reinterpret_cast<const ShapeSegmentQuadratic*>(segment)->distance(pos);
	case SHAPE_SEGMENT_TYPE_CUBIC: return reinterpret_cast<const ShapeSegmentCubic*>(segment)->distance(pos);
	default: return SignedDistance{};
	}
}
F32 shape_segment_pseudo_distance(const ShapeSegmentHeader* segment, V2F32 pos, F32 t) {
	switch (segment->type) {
	case SHAPE_SEGMENT_TYPE_LINEAR: return reinterpret_cast<const ShapeSegmentLinear*>(segment)->pseudo_distance(pos, t);
	case SHAPE_SEGMENT_TYPE_QUADRATIC: return reinterpret_cast<const ShapeSegmentQuadratic*>(segment)->pseudo_distance(pos, t);
	case SHAPE_SEGMENT_TYPE_CUBIC: return reinterpret_cast<const ShapeSegmentCubic*>(segment)->pseudo_distance(pos, t);
	default: return 0.0F;
	}
}

struct ShapePath {
	ShapePath* next;
	ShapePath* prev;
	ShapeSegmentHeader* shapeSegmentsBegin;
	ShapeSegmentHeader* shapeSegmentsEnd;
};

struct Shape {
	ShapePath* shapePathsBegin;
	ShapePath* shapePathsEnd;
};

template<typename T>
F32 bezier_pseudo_distance(const T& producer, V2F32 pos, F32 t) {
	V2F32 closest = producer.eval(t);
	V2F32 closestDir = producer.direction(t);
	F32 distance = length(closest - pos) * F32(signumf32(cross(closestDir, pos - closest)));

	V2F32 rayDir = producer.direction(1.0F);
	F32 rayDist = signed_distance_to_ray(pos, producer.eval(1.0F), rayDir);
	if (absf32(rayDist) < absf32(distance)) {
		distance = rayDist;
	}
	rayDir = -producer.direction(0.0F);
	rayDist = signed_distance_to_ray(pos, producer.eval(0.0F), -producer.direction(0.0F));
	if (absf32(rayDist) < absf32(distance)) {
		// Negate here because the ray is going the opposite direction, so winding order will also be opposite, making the sign opposite.
		distance = -rayDist;
	}
	return distance;
}

template<typename T>
SignedDistance numerical_distance_solve(V2F32 pos, const T& producer, F32 eps) {
	const U32 NUM_ITERATIONS = 32;

	// Check a bunch of points along the curve, find the closest one.
	SignedDistance minDist{ &producer.header, 0.0F, F32_LARGE, 0 };
	F32 iteration = 0.0F;
	for (U32 i = 0; i < NUM_ITERATIONS; i++) {
		F32 iN = F32(i) / F32(NUM_ITERATIONS - 1);
		V2F32 currentPos = producer.eval(iN);
		F32 currentDist = distance_sq(currentPos, pos);
		if (currentDist < minDist.distance) {
			minDist.distance = currentDist;
			iteration = iN;
		}
	}
	// Refine that point to within an acceptable error. Set a min and max boundary where the actual closest could be (otherwise it would have been another point)
	F32 minVal = max(0.0F, iteration - (1.0F / NUM_ITERATIONS));
	F32 maxVal = min(1.0F, iteration + (1.0F / NUM_ITERATIONS));
	F32 med = 0;
	// Binary search. Loop until the search area is within acceptable error.
	while ((maxVal - minVal) > eps) {
		med = (maxVal + minVal) * 0.5F;
		if (distance_sq(producer.eval(max(0.0F, med - eps)), pos) < distance_sq(producer.eval(min(1.0F, med + eps)), pos)) {
			maxVal = med;
		} else {
			minVal = med;
		}
	}
	// Make sure it's exactly one or zero if the min or max never changed from that. That way I don't have to deal with sligtly different floats.
	if (maxVal == 1.0F) {
		med = 1.0F;
	} else if (minVal == 0.0F) {
		med = 0.0F;
	}
	V2F32 closest = producer.eval(med);
	V2F32 closestToPos = normalize(pos - closest);
	V2F32 closestDir = producer.direction(med);
	F32 crossZ = cross(closestDir, closestToPos);
	minDist.distance = length(closest - pos);
	minDist.distance *= F32(signumf32(crossZ));
	minDist.orthogonality = absf32(crossZ);
	minDist.t = med;
	return minDist;
}


// https://www.w3.org/TR/SVG11/implnote.html#ArcConversionEndpointToCenter
void endpoint_arc_to_center_arc(V2F32* center, F32* angleStart, F32* angleRange, F32* newXRadius, F32* newYRadius, V2F32 currentPos, F32 xRadius, F32 yRadius, F32 xAxisRotation, B32 largeArcFlag, B32 sweepFlag, V2F32 nextPos) {
	M2F32 matrix;
	matrix.set_identity().rotate(xAxisRotation);
	M2F32 matrixTransposed;
	matrixTransposed.set(matrix).transpose();
	V2F32 midPos = matrix.transform((currentPos - nextPos) * 0.5);
	F32 sign = (largeArcFlag == sweepFlag) ? -1.0F : 1.0F;
	F32 xRadSq = xRadius * xRadius;
	F32 yRadSq = yRadius * yRadius;
	F32 midXSq = midPos.x * midPos.x;
	F32 midYSq = midPos.y * midPos.y;
	F32 alpha = (midXSq / xRadSq + midYSq / yRadSq);
	if (alpha > 1) {
		F32 sqrtAlpha = sqrtf32(alpha);
		xRadius = xRadius * sqrtAlpha;
		yRadius = yRadius * sqrtAlpha;
		*newXRadius = xRadius;
		*newYRadius = yRadius;
	}
	V2F32 cDerivative = V2F32{ (xRadius * midPos.y) / yRadius, (-yRadius * midPos.x) / xRadius } *(sign * sqrtf32((xRadSq * yRadSq - xRadSq * midYSq - yRadSq * midXSq) / (xRadSq * midYSq + yRadSq * midXSq)));
	*center = matrixTransposed * cDerivative + (currentPos + nextPos) * 0.5F;

	*angleStart = vec2_angle(V2F32{ 1, 0 }, V2F32{ (midPos.x - cDerivative.x) / xRadius, (midPos.y - cDerivative.y) / yRadius });
	if (sweepFlag && *angleStart < 0) {
		*angleStart += 1.0F;
	} else if (!sweepFlag && *angleStart > 0) {
		*angleStart -= 1.0F;
	}
	*angleRange = vec2_angle(V2F32{ (midPos.x - cDerivative.x) / xRadius, (midPos.y - cDerivative.y) / yRadius }, V2F32{ (-midPos.x - cDerivative.x) / xRadius, (-midPos.y - cDerivative.y) / yRadius });
}

void create_arc(MemoryArena& arena, ShapePath* pathToAddTo, V2F32 currentPos, F32 xRadius, F32 yRadius, F32 xAxisRotation, B32 largeArcFlag, B32 sweepFlag, V2F32 nextPos) {
	if (xRadius == 0 || yRadius == 0) {
		ShapeSegment* shapeSegment = arena.alloc<ShapeSegment>(1);
		shapeSegment->linear = ShapeSegmentLinear{ ShapeSegmentHeader{ SHAPE_SEGMENT_TYPE_LINEAR }, { currentPos, nextPos } };
		DLL_INSERT_TAIL(&shapeSegment->header, pathToAddTo->shapeSegmentsBegin, pathToAddTo->shapeSegmentsEnd, prev, next);
		return;
	}
	xRadius = absf32(xRadius);
	yRadius = absf32(yRadius);

	V2F32 center;
	F32 angleStart;
	F32 angleRange;
	endpoint_arc_to_center_arc(&center, &angleStart, &angleRange, &xRadius, &yRadius, currentPos, xRadius, yRadius, xAxisRotation, largeArcFlag, sweepFlag, nextPos);

	// It's kind of dumb to use a whole mat4x3 for this, but the original code did and it doesn't matter enough for me to put effort into changing it
	M4x3F32 transform;
	// A cubic bezier curve can represent a quarter turn with high precision, so use at most four to represent the arc
	U32 subdivisions = U32(ceilf32(absf32(angleRange) * 4.0F));
	F32 subdivRange = angleRange / F32(subdivisions);
	F32 sinAngleRange;
	F32 cosAngleRange = sincosf32(&sinAngleRange, subdivRange);
	F32 tanQuarterAngleRange = tanf32(subdivRange * 0.25F);
	const F32 fourthirds = 4.0F / 3.0F;
	for (U32 i = 0; i < subdivisions; i++) {
		F32 angle = xAxisRotation + angleStart + subdivRange * F32(i);
		// TODO I think this will be wrong for non circular ellipses, check it
		transform.set_identity().translate({ center.x, center.y, 0 }).rotate_axis_angle(V3F32{ 0, 0, 1 }, -angle).scale({ xRadius, yRadius, 1 });

		V2F32 bStart = (transform * V3F32{ 1, 0, 0 }).xy();
		V2F32 bHandle1 = (transform * V3F32{ 1, fourthirds * tanQuarterAngleRange, 0 }).xy();
		V2F32 bHandle2 = (transform * V3F32{ cosAngleRange + fourthirds * tanQuarterAngleRange * sinAngleRange, sinAngleRange - fourthirds * tanQuarterAngleRange * cosAngleRange, 0 }).xy();
		V2F32 bEnd = (transform * V3F32{ cosAngleRange, sinAngleRange, 0 }).xy();
		if (i == 0) {
			bStart = currentPos;
		}
		if (i == subdivisions - 1) {
			bEnd = nextPos;
		}
		ShapeSegment* shapeSegment = arena.alloc<ShapeSegment>(1);
		shapeSegment->cubic = ShapeSegmentCubic{ ShapeSegmentHeader{ SHAPE_SEGMENT_TYPE_CUBIC }, { bStart, bHandle1, bHandle2, bEnd } };
		DLL_INSERT_TAIL(&shapeSegment->header, pathToAddTo->shapeSegmentsBegin, pathToAddTo->shapeSegmentsEnd, prev, next);
	}
}


Shape parse_shape(MemoryArena& arena, StrA shapeStr) {
	Shape shape{};
	ShapePath* path = nullptr;
	StrA cmdStr{};
	for (I64 i = 0; i < I64(shapeStr.length); i++) {
		if (shapeStr.skip(i).starts_with("d=\"M"sa) || shapeStr.skip(i).starts_with("d=\"m"sa)) {
			cmdStr = shapeStr.skip(i + 3);
			break;
		}
		if (shapeStr.skip(i).starts_with("</svg>"sa)) {
			break;
		}
	}
	if (cmdStr.str == nullptr) {
		return shape;
	}
	for (U64 i = 0; i < cmdStr.length; i++) {
		if (cmdStr.str[i] == '"') {
			cmdStr.length = i;
			break;
		}
	}

	MemoryArena& stackArena = get_scratch_arena_excluding(arena);
	U64 oldStackArenaPtr = stackArena.stackPtr;

	ArenaArrayList<F32> args{ &stackArena };
	V2F32 currentPos{};
	char insn{};
	while (!cmdStr.is_empty()) {
		insn = cmdStr[0];
		cmdStr = cmdStr.skip(1ull);
		if (insn == ' ') {
			continue;
		}
		B32 relative = false;
		if (insn >= 'a' && insn <= 'z') {
			relative = true;
			insn -= ('a' - 'A');
		}

		args.clear();
		F32 arg;
		while (ParseTools::parse_f32(&arg, &cmdStr)) {
			args.push_back(arg);
			while (cmdStr.length && (ParseTools::is_whitespace(cmdStr[0]) || cmdStr[0] == ',')) {
				cmdStr.str++, cmdStr.length--;
			}
		}

		switch (insn) {
		case 'M': {
			if (path) {
				DLL_INSERT_HEAD(path, shape.shapePathsBegin, shape.shapePathsEnd, prev, next);
			}
			path = arena.zalloc<ShapePath>(1);
			if (!relative) {
				currentPos = V2F32{};
			}
			currentPos += V2F32{ args.data[0], args.data[1] };
			for (U32 i = 2; i < args.size; i += 2) {
				V2F32 endPos{ args.data[i], args.data[i + 1] };
				if (relative) {
					endPos += currentPos;
				}
				ShapeSegment* shapeSegment = arena.alloc<ShapeSegment>(1);
				shapeSegment->linear = ShapeSegmentLinear{ ShapeSegmentHeader{ SHAPE_SEGMENT_TYPE_LINEAR }, { currentPos, endPos } };
				DLL_INSERT_TAIL(&shapeSegment->header, path->shapeSegmentsBegin, path->shapeSegmentsEnd, prev, next);
				if (length_sq(currentPos - endPos) < 0.0001F) {
					println("MSDF warning: degenerate linear segment");
				}
				currentPos = endPos;
			}
		} break;
		case 'H': {
			for (U32 i = 0; i < args.size; i++) {
				V2F32 endPos{ args.data[i], currentPos.y };
				if (relative) {
					endPos.x += currentPos.x;
				}
				ShapeSegment* shapeSegment = arena.alloc<ShapeSegment>(1);
				shapeSegment->linear = ShapeSegmentLinear{ ShapeSegmentHeader{ SHAPE_SEGMENT_TYPE_LINEAR }, { currentPos, endPos } };
				DLL_INSERT_TAIL(&shapeSegment->header, path->shapeSegmentsBegin, path->shapeSegmentsEnd, prev, next);
				if (length_sq(currentPos - endPos) < 0.0001F) {
					println("MSDF warning: degenerate linear segment");
				}
				currentPos = endPos;
			}
		} break;
		case 'V': {
			for (U32 i = 0; i < args.size; i++) {
				V2F32 endPos{ currentPos.x, args.data[i] };
				if (relative) {
					endPos.y += currentPos.y;
				}
				ShapeSegment* shapeSegment = arena.alloc<ShapeSegment>(1);
				shapeSegment->linear = ShapeSegmentLinear{ ShapeSegmentHeader{ SHAPE_SEGMENT_TYPE_LINEAR }, { currentPos, endPos } };
				DLL_INSERT_TAIL(&shapeSegment->header, path->shapeSegmentsBegin, path->shapeSegmentsEnd, prev, next);
				if (length_sq(currentPos - endPos) < 0.0001F) {
					println("MSDF warning: degenerate linear segment");
				}
				currentPos = endPos;
			}
		} break;
		case 'L': {
			for (U32 i = 0; i < args.size; i += 2) {
				V2F32 endPos{ args.data[i], args.data[i + 1] };
				if (relative) {
					endPos += currentPos;
				}
				ShapeSegment* shapeSegment = arena.alloc<ShapeSegment>(1);
				shapeSegment->linear = ShapeSegmentLinear{ ShapeSegmentHeader{ SHAPE_SEGMENT_TYPE_LINEAR }, { currentPos, endPos } };
				DLL_INSERT_TAIL(&shapeSegment->header, path->shapeSegmentsBegin, path->shapeSegmentsEnd, prev, next);
				if (length_sq(currentPos - endPos) < 0.0001F) {
					println("MSDF warning: degenerate linear segment");
				}
				currentPos = endPos;
			}
		} break;
		case 'Z': {
			V2F32 endPos = shape_segment_eval(path->shapeSegmentsBegin, 0.0F);
			if (length_sq(currentPos - endPos) > 0.01F) {
				// Inkscape will generate a Z connection even if the previous line already would have gotten there
				ShapeSegment* shapeSegment = arena.alloc<ShapeSegment>(1);
				shapeSegment->linear = ShapeSegmentLinear{ ShapeSegmentHeader{ SHAPE_SEGMENT_TYPE_LINEAR }, { currentPos, endPos } };
				DLL_INSERT_TAIL(&shapeSegment->header, path->shapeSegmentsBegin, path->shapeSegmentsEnd, prev, next);
				currentPos = endPos;
			}
		} break;
		case 'Q': {
			for (U32 i = 0; i < args.size; i += 4) {
				V2F32 handle{ args.data[i], args.data[i + 1] };
				V2F32 endPos{ args.data[i + 2], args.data[i + 3] };
				if (relative) {
					handle += currentPos;
					endPos += currentPos;
				}
				ShapeSegment* shapeSegment = arena.alloc<ShapeSegment>(1);
				if (distance_sq(handle, currentPos) < 0.0001F || distance_sq(handle, endPos) < 0.0001F) {
					shapeSegment->linear = ShapeSegmentLinear{ ShapeSegmentHeader{ SHAPE_SEGMENT_TYPE_LINEAR }, { currentPos, endPos } };
				} else {
					shapeSegment->quadratic = ShapeSegmentQuadratic{ ShapeSegmentHeader{ SHAPE_SEGMENT_TYPE_QUADRATIC }, { currentPos, handle, endPos } };
				}
				DLL_INSERT_TAIL(&shapeSegment->header, path->shapeSegmentsBegin, path->shapeSegmentsEnd, prev, next);
				currentPos = endPos;
			}
		} break;
		case 'C': {
			for (U32 i = 0; i < args.size; i += 6) {
				V2F32 handle1{ args.data[i], args.data[i + 1] };
				V2F32 handle2{ args.data[i + 2], args.data[i + 3] };
				V2F32 endPos{ args.data[i + 4], args.data[i + 5] };
				if (relative) {
					handle1 += currentPos;
					handle2 += currentPos;
					endPos += currentPos;
				}
				ShapeSegment* shapeSegment = arena.alloc<ShapeSegment>(1);
				shapeSegment->cubic = ShapeSegmentCubic{ ShapeSegmentHeader{ SHAPE_SEGMENT_TYPE_CUBIC }, { currentPos, handle1, handle2, endPos } };
				DLL_INSERT_TAIL(&shapeSegment->header, path->shapeSegmentsBegin, path->shapeSegmentsEnd, prev, next);
				currentPos = endPos;
			}
		} break;
		case 'A': {
			for (U32 i = 0; i < args.size; i += 7) {
				F32 xRadius = args.data[i];
				F32 yRadius = args.data[i + 1];
				F32 xAxisRotationDegrees = args.data[i + 2];
				B32 largeArcFlag = args.data[i + 3] != 0;
				B32 sweepFlag = args.data[i + 4] != 0;
				V2F32 nextPos = { args.data[i + 5] , args.data[i + 6] };
				if (relative) {
					nextPos += currentPos;
				}
				create_arc(arena, path, currentPos, xRadius, yRadius, DEG_TO_TURN(xAxisRotationDegrees), largeArcFlag, sweepFlag, nextPos);
				currentPos = nextPos;
			}
		} break;
		default: {
			print("Unknown SVG opcode: ");
			print(StrA{ &insn, 1 });
			println();
		} break;
		}
	}
	DLL_INSERT_HEAD(path, shape.shapePathsBegin, shape.shapePathsEnd, prev, next);
	stackArena.stackPtr = oldStackArenaPtr;
	return shape;
}

void preprocess_shape(Shape shape) {
	const F32 SMOOTH_CUTOFF = 0.001F;
	for (ShapePath* p = shape.shapePathsBegin; p != nullptr; p = p->next) {
		RGBA8 color = p->shapeSegmentsBegin == p->shapeSegmentsEnd ? RGBA8{ 255, 255, 255 } : RGBA8{ 255, 0, 255 };
		if (p->shapeSegmentsBegin == p->shapeSegmentsEnd && absf32(cross(shape_segment_direction(p->shapeSegmentsBegin, 0.0F), shape_segment_direction(p->shapeSegmentsBegin, 1.0F))) < SMOOTH_CUTOFF) {
			p->shapeSegmentsBegin->color[0] = RGBA8{ 255, 255, 0 };
			p->shapeSegmentsBegin->color[1] = RGBA8{ 0, 255, 255 };
		} else {
			ShapeSegmentHeader* prev = nullptr;
			for (ShapeSegmentHeader* s = p->shapeSegmentsBegin; s != nullptr; s = s->next) {
				if (prev) {
					if (color == RGBA8{ 255, 255, 0 }) {
						color = RGBA8{ 0, 255, 255 };
					} else {
						color = RGBA8{ 255, 255, 0 };
					}
				}
				s->color[1] = s->color[0] = color;
				prev = s;
			}
		}
	}
}

F32 median(RGBA8 pixel) {
	return F32(max(min(pixel.r, pixel.g), min(max(pixel.r, pixel.g), pixel.b))) / 255.0F;
}

void render_shape_to_msdf(RGBA8* pixels, Shape shape, U32 width, U32 height, F32 svgWidth, F32 svgHeight, F32 radius) {
	radius = max(1.0F, min(radius, F32(width / 2), F32(height / 2)));
	F32 invRadius = 1.0F / radius;
	V2F32 halfSvg{ svgWidth * 0.5F, svgHeight * 0.5F };
	V2F32 halfDst{ F32(width) * 0.5F, F32(height) * 0.5F };
	// To scale destination pixel to svg unit
	F32 scaleFactor = halfSvg.x / (halfDst.x - radius);
	if ((halfDst.y - radius) * scaleFactor < halfSvg.y) {
		scaleFactor = halfSvg.y / (halfDst.y - radius);
	}
	// To scale svg distance to pixel distance
	F32 invScaleFactor = 1.0F / scaleFactor;
	V2F32 offsetFactor = (0.5F - halfDst) * scaleFactor + halfSvg;

	for (U32 y = 0; y < height; y++) {
		for (U32 x = 0; x < width; x++) {
			V2F32 pos = V2F32{ F32(x), F32(y) } *scaleFactor + offsetFactor;
			SignedDistance sRed{ nullptr, 0.0F, F32_LARGE, -F32_LARGE };
			SignedDistance sGreen{ nullptr, 0.0F, F32_LARGE, -F32_LARGE };
			SignedDistance sBlue{ nullptr, 0.0F, F32_LARGE, -F32_LARGE };

			for (ShapePath* path = shape.shapePathsBegin; path != nullptr; path = path->next) {
				for (ShapeSegmentHeader* seg = path->shapeSegmentsBegin; seg != nullptr; seg = seg->next) {
					SignedDistance dist = shape_segment_distance(seg, pos);
					RGBA8 color = dist.segment->color_at(dist.t);

					if (color.r > 0 && dist.abs() < sRed.abs()) {
						sRed = dist;
					}
					if (color.g > 0 && dist.abs() < sGreen.abs()) {
						sGreen = dist;
					}
					if (color.b > 0 && dist.abs() < sBlue.abs()) {
						sBlue = dist;
					}
				}
			}

			RGBA8 finalColor{ 0, 0, 0, 255 };
			if (sRed.segment) {
				finalColor.r = U8((1.0F - clamp01(shape_segment_pseudo_distance(sRed.segment, pos, sRed.t) * invScaleFactor * invRadius * 0.5F + 0.5F)) * 255);
			}
			if (sGreen.segment) {
				finalColor.g = U8((1.0F - clamp01(shape_segment_pseudo_distance(sGreen.segment, pos, sGreen.t) * invScaleFactor * invRadius * 0.5F + 0.5F)) * 255);
			}
			if (sBlue.segment) {
				finalColor.b = U8((1.0F - clamp01(shape_segment_pseudo_distance(sBlue.segment, pos, sBlue.t) * invScaleFactor * invRadius * 0.5F + 0.5F)) * 255);
			}
			pixels[y * width + x] = finalColor;
		}
	}
}

// Error correction concept from MSDFGen
B32 detect_error_pixel_pair(RGBA8 a, RGBA8 b, U8 threshold) {
	if (b.r == b.g && b.r == b.b) {
		// Other was already corrected
		return false;
	}
	// Sort from biggest to smallest 
	if (abs(a.r - b.r) < abs(a.g - b.g)) {
		swap(&a.r, &a.g);
		swap(&b.r, &b.g);
	}
	if (abs(a.g - b.g) < abs(a.b - b.b)) {
		swap(&a.g, &a.b);
		swap(&b.g, &b.b);
		if (abs(a.r - b.r) < abs(a.g - b.g)) {
			swap(&a.r, &a.g);
			swap(&b.r, &b.g);
		}
	}
	return abs(a.g - b.g) >= threshold &&
		abs(a.b - 127) >= abs(b.b - 127); // Only flag the one further from a shape edge
}

void error_correct(RGBA8* image, U32 width, U32 height) {
	U8 thresholdX = 20;
	U8 thresholdY = 20;
	U8 thersholdXY = U8(thresholdX + thresholdY);
	for (U32 y = 0; y < height; y++) {
		for (U32 x = 0; x < width; x++) {
			if (x > 0 && detect_error_pixel_pair(image[y * width + x], image[y * width + x - 1], thresholdX) ||
				x < width - 1 && detect_error_pixel_pair(image[y * width + x], image[y * width + x + 1], thresholdX) ||
				y > 0 && detect_error_pixel_pair(image[y * width + x], image[(y - 1) * width + x], thresholdY) ||
				y < height - 1 && detect_error_pixel_pair(image[y * width + x], image[(y + 1) * width + x], thresholdY) ||
				x > 0 && y > 0 && detect_error_pixel_pair(image[y * width + x], image[(y - 1) * width + x - 1], thersholdXY) ||
				x < width - 1 && y > 0 && detect_error_pixel_pair(image[y * width + x], image[(y - 1) * width + x + 1], thersholdXY) ||
				x > 0 && y < height - 1 && detect_error_pixel_pair(image[y * width + x], image[(y + 1) * width + x - 1], thersholdXY) ||
				x < width - 1 && y < height - 1 && detect_error_pixel_pair(image[y * width + x], image[(y + 1) * width + x + 1], thersholdXY)) {

				RGBA8 pixel = image[y * width + x];
				U8 med = U8(median(pixel) * 255.0F);
				pixel.r = med;
				pixel.g = med;
				pixel.b = med;
				image[y * width + x] = pixel;
			}
		}
	}
}

struct MSDFImageFile {
	U32 imageWidth;
	U32 imageHeight;
	RGBA8 imageData[0];
};

void generate_msdf_image(StrA inputFilePath, StrA outputFilePath, U32 sizeX, U32 sizeY, F32 svgWidth, F32 svgHeight, F32 radius) {
	MemoryArena& stackArena = get_scratch_arena();
	U64 oldStackPtr = stackArena.stackPtr;
	U32 srcFileSize;
	char* inputFile = read_full_file_to_arena<char>(&srcFileSize, stackArena, inputFilePath);
	if (inputFile == nullptr) {
		println("Failed to read input file for MSDF font");
		stackArena.stackPtr = oldStackPtr;
		return;
	}
	StrA inputStr{ inputFile, srcFileSize };

	U32 outputFileSize = OFFSET_OF(MSDFImageFile, imageData[sizeX * sizeY * sizeof(RGBA8)]);
	MSDFImageFile* output = stackArena.alloc_bytes<MSDFImageFile>(outputFileSize);
	zero_memory(output, outputFileSize);

	I64 nextGlyphLayer = inputStr.find("id=\"layer1\""sa);
	if (nextGlyphLayer == -1) {
		println("Could not find layer1 in file");
		stackArena.stackPtr = oldStackPtr;
		return;
	}
	inputStr.skip(nextGlyphLayer + I64("id=\"layer1\""sa.length));
	Shape shape = parse_shape(stackArena, inputStr);
	preprocess_shape(shape);
	render_shape_to_msdf(output->imageData, shape, sizeX, sizeY, svgWidth, svgHeight, radius);
	error_correct(output->imageData, sizeX, sizeY);

	output->imageWidth = sizeX;
	output->imageHeight = sizeY;
	B32 success = write_data_to_file(outputFilePath, output, outputFileSize);
	if (!success) {
		print("Failed to write out MSDF result\n");
	}

	stackArena.stackPtr = oldStackPtr;
}

void generate_msdf_font(StrA inputFilePath, StrA outputFilePath, U32 glyphResolutionX, U32 glyphResolutionY, F32 svgWidth, F32 svgHeight, F32 radius, B32 debug = false) {
	MemoryArena& stackArena = get_scratch_arena();
	U64 oldStackPtr = stackArena.stackPtr;
	U32 srcFileSize;
	char* inputFile = read_full_file_to_arena<char>(&srcFileSize, stackArena, inputFilePath);
	if (inputFile == nullptr) {
		println("Failed to read input file for MSDF font");
		stackArena.stackPtr = oldStackPtr;
		return;
	}
	StrA inputStr{ inputFile, srcFileSize };

	U32 outputFileSize = OFFSET_OF(MSDFImageFile, imageData[glyphResolutionX * glyphResolutionY * sizeof(RGBA8) * U8_MAX]);
	MSDFImageFile* output = stackArena.alloc_bytes<MSDFImageFile>(outputFileSize);
	zero_memory(output, outputFileSize);
	RGBA8* tmpGlyphOutput = stackArena.alloc<RGBA8>(glyphResolutionX * glyphResolutionY);

	while (true) {
		I64 nextGlyphLayer = inputStr.find("GlyphLayer-"sa);
		if (nextGlyphLayer == -1) {
			break;
		}
		inputStr = inputStr.skip(nextGlyphLayer + I64("GlyphLayer-"sa.length));
		char glyph = inputStr[0];
		if (inputStr.starts_with("&lt;"sa)) {
			glyph = '<';
		} else if (inputStr.starts_with("&gt;"sa)) {
			glyph = '>';
		} else if (inputStr.starts_with("&quot;"sa)) {
			glyph = '"';
		} else if (inputStr.starts_with("&amp;"sa)) {
			glyph = '&';
		}
		print(stracat(stackArena, "Generating shape for glyph: "sa, StrA{ &glyph, 1 }, "\n"sa));
		Shape shape = parse_shape(stackArena, inputStr);
		preprocess_shape(shape);
		render_shape_to_msdf(tmpGlyphOutput, shape, glyphResolutionX, glyphResolutionY, svgWidth, svgHeight, radius);
		error_correct(tmpGlyphOutput, glyphResolutionX, glyphResolutionY);

		for (U32 y = 0; y < glyphResolutionY; y++) {
			U32 dstRowOffset = (glyphResolutionY * (U32(glyph) >> 4) + y) * glyphResolutionX * 16 + (U32(glyph) & 0xF) * glyphResolutionX;
			for (U32 x = 0; x < glyphResolutionX; x++) {
				output->imageData[dstRowOffset + x] = tmpGlyphOutput[y * glyphResolutionY + x];
			}
		}
	}

	if (debug) {
		PNG::write_image(stracat(stackArena, outputFilePath, "_full_atlas.png"sa), output->imageData, glyphResolutionX * 16, glyphResolutionY * 16);
		output->imageWidth = glyphResolutionX * 16;
		output->imageHeight = glyphResolutionY * 16;
	}
	output->imageWidth = glyphResolutionX * 16;
	output->imageHeight = glyphResolutionY * 16;
	B32 success = write_data_to_file(outputFilePath, output, outputFileSize);
	if (!success) {
		print("Failed to write out MSDF result\n");
	}

	stackArena.stackPtr = oldStackPtr;
}

}