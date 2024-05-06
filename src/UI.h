#pragma once
#include "DrillLib.h"
#include "Textures.h"
#include "DynamicVertexBuffer.h"
#include "TextRenderer.h"

namespace UI {

RWSpinLock modificationLock;

ArenaArrayList<V2F32> sizeStack;
ArenaArrayList<RGBA8> textColorStack;
ArenaArrayList<RGBA8> borderColorStack;
ArenaArrayList<RGBA8> backgroundColorStack;
ArenaArrayList<F32> textSizeStack;
ArenaArrayList<F32> borderWidthStack;
ArenaArrayList<F32> textBorderSpacingStack;
ArenaArrayList<Flags32> defaultFlagsStack;

#define UI_SIZE(newSize) DEFER_LOOP(sizeStack.push_back(newSize), sizeStack.pop_back())
#define UI_BACKGROUND_COLOR(newColor) DEFER_LOOP(backgroundColorStack.push_back((newColor).to_rgba8()), backgroundColorStack.pop_back())
#define UI_TEXT_COLOR(newColor) DEFER_LOOP(textColorStack.push_back((newColor).to_rgba8()), textColorStack.pop_back())
#define UI_BORDER_COLOR(newColor) DEFER_LOOP(borderColorStack.push_back((newColor).to_rgba8()), borderColorStack.pop_back())
#define UI_TEXT_SIZE(newSize) DEFER_LOOP(textSizeStack.push_back(newSize), textSizeStack.pop_back())
#define UI_BORDER_WIDTH(newWidth) DEFER_LOOP(borderWidthStack.push_back(newWidth), borderWidthStack.pop_back())
#define UI_TEXT_BORDER_SPACING(newSpacing) DEFER_LOOP(textBorderSpacingStack.push_back(newSpacing), textBorderSpacingStack.pop_back())
#define UI_FLAGS(newFlags) DEFER_LOOP(defaultFlagsStack.push_back(newFlags), defaultFlagsStack.pop_back())

#define UI_WORKING_BOX(newBox) for (UI::BoxHandle oldWorkingBox = UI::workingBox, irrelevant = UI::workingBox = (newBox); oldWorkingBox.unsafeBox; UI::workingBox = oldWorkingBox, oldWorkingBox.unsafeBox = nullptr)

const U32 MAX_CLIP_BOXES = 0x10000;
ArenaArrayList<U32> clipBoxIndexStack;
ArenaArrayList<Rng2F32> clipBoxStack;
VK::DedicatedBuffer clipBoxBuffers[VK::FRAMES_IN_FLIGHT];
U32 currentClipBoxCount;

const U32 MAX_TEXT_INPUT = 512;

enum LayoutDirection : U32 {
	LAYOUT_DIRECTION_NONE,
	LAYOUT_DIRECTION_UP,
	LAYOUT_DIRECTION_DOWN,
	LAYOUT_DIRECTION_LEFT,
	LAYOUT_DIRECTION_RIGHT
};
struct Box;
// Pretty much an amagamation of anything interesting that might happen to a box.
// Each of these fields can be optionally handled in the action callback.
struct UserCommunication {
	V2F32 mousePos;
	F32 scrollInput;
	Flags32 leftClicked : 1;
	Flags32 rightClicked : 1;
	Flags32 middleClicked : 1;
	Flags32 mouse4Clicked : 1;
	Flags32 mouse5Clicked : 1;
	Flags32 leftClickStart : 1;
	Flags32 rightClickStart : 1;
	Flags32 middleClickStart : 1;
	Flags32 mouse4ClickStart : 1;
	Flags32 mouse5ClickStart : 1;
	Flags32 mouseHovered : 1;
	Box* draggedTo;
	V2F32 drag;
	Win32::Key keyPressed;
	char charTyped;
	DynamicVertexBuffer::Tessellator* tessellator;
	Rng2F32 renderArea;
	Rng2F32 renderClipBox;
	U32 clipBoxIndex;
	F32 renderZ;
	F32 scale;
	B32 boxIsBeingDestroyed;
};
enum ActionResult : U32 {
	ACTION_PASS,
	ACTION_HANDLED
};
typedef ActionResult (*BoxActionCallback)(Box* box, UserCommunication& com);
typedef void (*BoxConsumer)(Box* box);
enum BoxFlag : U32 {
	BOX_FLAG_INVISIBLE = (1 << 0),
	BOX_FLAG_DISABLED = (1 << 1),
	BOX_FLAG_SPACER = (1 << 2),
	BOX_FLAG_FLOATING_X = (1 << 3),
	BOX_FLAG_FLOATING_Y = (1 << 4),
	BOX_FLAG_DONT_LAYOUT_TO_FIT_CHILDREN = (1 << 5),
	BOX_FLAG_RESET_LAYOUT = (1 << 6),
	BOX_FLAG_CLIP_CHILDREN = (1 << 7),
	BOX_FLAG_HIGHLIGHT_ON_USER_INTERACTION = (1 << 8),
	BOX_FLAG_CUSTOM_DRAW = (1 << 9),
	BOX_FLAG_CENTER_ON_ORTHOGONAL_AXIS = (1 << 10),
	BOX_FLAG_DONT_CLOSE_CONTEXT_MENU_ON_INTERACTION = (1 << 11)
};
typedef Flags32 BoxFlags;
const F32 BOX_INF_SIZE = 100000.0F;
// Having such a large box struct might be memory inefficient, but this setup makes it very easy to combine features, and the total UI memory usage isn't even that much.
// If there are 10000 boxes on the screen, that's only a couple megabytes total.
struct Box {
	BoxFlags flags;
	LayoutDirection layoutDirection;
	U64 generation;

	// Up the box tree
	Box* parent;
	// Forms a linked list with boxes at the current level of the tree
	Box* next;
	Box* prev;
	// Links to the children
	Box* childFirst;
	Box* childLast;

	V2F32 computedSize;
	V2F32 computedMinSize;
	V2F32 computedParentRelativeOffset;
	V2F32 computedOffset;

	V2F32 minSize;
	V2F32 idealSize;
	V2F32 sizeParentPercent;

	// Used for things like scroll bars and node editors that can zoom and move around
	V2F32 contentOffset;
	F32 contentScale;

	F32 zOffset;

	StrA text;
	StrA tooltipString;
	// If typedTextBuffer is not null, text acts as a prompt for the user to type
	char* typedTextBuffer;
	U32 numTypedCharacters;
	F32 textSize;
	F32 textBorderSpacing;
	F32 borderWidth;
	Win32::CursorType hoverCursor;
	RGBA8 textColor;
	RGBA8 borderColor;
	RGBA8 backgroundColor;
	Textures::Texture* backgroundTexture;

	BoxActionCallback actionCallback;
	
	U64 userTypeId;
	U64 userData[4];
};
struct BoxHandle {
	Box* unsafeBox;
	U64 generation;

	FINLINE Box* get() {
		return !unsafeBox || unsafeBox->generation == 0 || unsafeBox->generation > generation ? nullptr : unsafeBox;
	}
};

BoxHandle root;
BoxHandle workingBox;

ArenaArrayList<BoxHandle> contextMenuStack;
BoxHandle tooltip;

// Hot is an element a user is about to interact with (mouse hovering over, keyboard selected it with tab, etc)
BoxHandle hotBox;
F64 hotBoxStartTimeSeconds;
// Active is an element a user started an interaction with (mouse click down, keyboard enter down)
BoxHandle activeBox;
F32 activeBoxTotalScale;
// The box currently selected for typing
BoxHandle activeTextBox;

U64 currentGeneration = 1;
Box* boxFreeList = nullptr;

char* textInputFreeList = nullptr;

void make_default_settings(Box* box) {
	box->flags = defaultFlagsStack.back();
	box->idealSize = V2F32{};
	box->minSize = sizeStack.back();
	box->sizeParentPercent = V2F32{};
	box->text = StrA{};

	box->contentOffset = V2F32{};
	box->contentScale = 1.0F;

	box->zOffset = 0.0F;
	
	box->textSize = textSizeStack.back();
	box->textBorderSpacing = textBorderSpacingStack.back();
	box->borderWidth = borderWidthStack.back();
	box->hoverCursor = Win32::CURSOR_TYPE_POINTER;
	box->textColor = textColorStack.back();
	box->borderColor = borderColorStack.back();
	box->backgroundColor = backgroundColorStack.back();
	box->backgroundTexture = nullptr;
	box->actionCallback = nullptr;
	for (U32 i = 0; i < ARRAY_COUNT(box->userData); i++) {
		box->userData[i] = 0;
	}
	box->layoutDirection = LAYOUT_DIRECTION_DOWN;
}

char* alloc_text_input() {
	if (textInputFreeList == nullptr) {
		textInputFreeList = globalArena.alloc<char>(MAX_TEXT_INPUT);
		STORE_LE64(textInputFreeList, 0);
	}
	char* result = textInputFreeList;
	textInputFreeList = reinterpret_cast<char*>(LOAD_LE64(result));
	return result;
}
void free_text_input(char* buffer) {
	STORE_LE64(buffer, UPtr(textInputFreeList));
	textInputFreeList = buffer;
}
BoxHandle alloc_box() {
	if (!boxFreeList) {
		const U32 amountToAlloc = 256;
		boxFreeList = globalArena.alloc<Box>(amountToAlloc);
		for (U32 i = 0; i < amountToAlloc; i++) {
			boxFreeList[i].next = &boxFreeList[i + 1];
		}
		boxFreeList[amountToAlloc - 1].next = nullptr;
	}
	Box* box = boxFreeList;
	*box = Box{};
	make_default_settings(box);
	box->generation = currentGeneration++;
	boxFreeList = box->next;
	return BoxHandle{ box, box->generation };
}
void free_box(BoxHandle boxHandle) {
	if (Box* box = boxHandle.get()) {
		if (box->actionCallback) {
			UserCommunication comm{};
			comm.boxIsBeingDestroyed = true;
			box->actionCallback(box, comm);
		}
		for (Box* child = box->childFirst; child; child = child->next) {
			free_box(BoxHandle{ child, child->generation });
		}
		if (box->parent) {
			DLL_REMOVE(box, box->parent->childFirst, box->parent->childLast, prev, next);
		}
		if (box->typedTextBuffer) {
			free_text_input(box->typedTextBuffer);
		}
		box->parent = nullptr;
		box->generation = 0;
		box->next = boxFreeList;
		boxFreeList = box;
	}
}

void move_box_to_front(Box* box) {
	if (box->parent) {
		DLL_REMOVE(box, box->parent->childFirst, box->parent->childLast, prev, next);
		DLL_INSERT_HEAD(box, box->parent->childFirst, box->parent->childLast, prev, next);
	}
}

void context_menu(BoxHandle parent, BoxHandle box, V2F32 pos) {
	U32 newContextMenuStackSize = contextMenuStack.size;
	if (Box* parentBox = parent.get()) {
		for (I32 i = I32(contextMenuStack.size) - 1; i >= 0; i--) {
			Box* contextMenuCurrentBox = contextMenuStack.data[i].get();
			if (contextMenuCurrentBox == nullptr) {
				newContextMenuStackSize = i;
			} else if (contextMenuCurrentBox == parentBox) {
				newContextMenuStackSize = i + 1;
			}
		}
	} else {
		newContextMenuStackSize = 0;
	}
	if (!box.get()) {
		newContextMenuStackSize = 0;
	}
	for (U32 i = newContextMenuStackSize; i < contextMenuStack.size; i++) {
		free_box(contextMenuStack.data[i]);
	}
	contextMenuStack.resize(newContextMenuStackSize);
	if (Box* newMenu = box.get()) {
		newMenu->contentOffset = pos;
		newMenu->flags |= BOX_FLAG_FLOATING_X | BOX_FLAG_FLOATING_Y;
		contextMenuStack.push_back(box);
	}
}

void clear_context_menu() {
	context_menu(BoxHandle{}, BoxHandle{}, V2F32{});
}

void init_ui() {
	sizeStack.reserve(16);
	sizeStack.push_back(V2F32{ 16.0F, 16.0F });
	textColorStack.reserve(16);
	textColorStack.push_back(RGBA8{ 255, 255, 255, 255 });
	borderColorStack.reserve(16);
	borderColorStack.push_back(RGBA8{ 245, 155, 59, 255 });
	backgroundColorStack.reserve(16);
	backgroundColorStack.push_back(RGBA8{ 61, 61, 61, 255 });
	textSizeStack.reserve(16);
	textSizeStack.push_back(12.0F);
	borderWidthStack.reserve(16);
	borderWidthStack.push_back(2.0F);
	textBorderSpacingStack.reserve(16);
	textBorderSpacingStack.push_back(4.0F);
	defaultFlagsStack.reserve(16);
	defaultFlagsStack.push_back(0);

	contextMenuStack.reserve(16);

	workingBox = root = alloc_box();
	root.unsafeBox->flags |= BOX_FLAG_DONT_LAYOUT_TO_FIT_CHILDREN | BOX_FLAG_INVISIBLE;
	root.unsafeBox->zOffset = 1000.0F;

	for (U32 i = 0; i < VK::FRAMES_IN_FLIGHT; i++) {
		clipBoxBuffers[i].create(MAX_CLIP_BOXES * sizeof(Rng2F32), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VK::hostMemoryTypeIndex);
	}
}

void destroy_ui() {
	for (U32 i = 0; i < VK::FRAMES_IN_FLIGHT; i++) {
		clipBoxBuffers[i].destroy();
	}
}

void layout_boxes_recurse(Box* box) {
	box->computedSize = box->idealSize;
	box->computedMinSize = box->minSize;
	if (!box->text.is_empty() && !(box->flags & BOX_FLAG_DONT_LAYOUT_TO_FIT_CHILDREN)) {
		box->computedSize.y = max(box->computedSize.y, box->textSize) + box->textBorderSpacing * 2.0F;
		box->computedMinSize.y = max(box->computedMinSize.y, box->computedSize.y);
		box->computedSize.x = max(box->computedSize.x, TextRenderer::string_size_x(box->text, box->textSize)) + box->textBorderSpacing * 2.0F;
		box->computedMinSize.x = max(box->computedMinSize.x, box->computedSize.x);
	}
	if (box->sizeParentPercent.x != 0.0F || box->sizeParentPercent.y != 0.0F) {
		Box* firstIndependentlySizedParent = box->parent;
		while (firstIndependentlySizedParent && !(firstIndependentlySizedParent->flags & BOX_FLAG_DONT_LAYOUT_TO_FIT_CHILDREN)) {
			firstIndependentlySizedParent = firstIndependentlySizedParent->parent;
		}
		if (firstIndependentlySizedParent) {
			if (box->sizeParentPercent.x != 0.0F) {
				box->computedSize.x = max(box->computedSize.x, firstIndependentlySizedParent->computedSize.x * box->sizeParentPercent.x);
			}
			if (box->sizeParentPercent.y != 0.0F) {
				box->computedSize.y = max(box->computedSize.y, firstIndependentlySizedParent->computedSize.y * box->sizeParentPercent.y);
			}
		}
	}
	box->computedSize.x = max(box->computedMinSize.x, box->computedSize.x);
	box->computedSize.y = max(box->computedMinSize.y, box->computedSize.y);

	for (Box* child = box->childFirst; child; child = child->next) {
		layout_boxes_recurse(child);
	}

	if (!(box->flags & BOX_FLAG_DONT_LAYOUT_TO_FIT_CHILDREN)) {
		Axis2 layoutAxis = box->layoutDirection == LAYOUT_DIRECTION_UP || box->layoutDirection == LAYOUT_DIRECTION_DOWN ? AXIS2_Y : AXIS2_X;
		Axis2 orthogonalAxis = layoutAxis == AXIS2_X ? AXIS2_Y : AXIS2_X;

		F32 current = 0.0F;
		F32 maxAxis = 0.0F;
		F32 maxOrthogonal = 0.0F;
		F32 currentMin = 0.0F;
		F32 maxAxisMin = 0.0F;
		F32 maxOrthogonalMin = 0.0F;
		for (Box* child = box->childFirst; child; child = child->next) {
			if (child->flags & BOX_FLAG_RESET_LAYOUT) {
				currentMin = current = 0.0F;
			}
			if (!(child->flags & BOX_FLAG_FLOATING_X << layoutAxis)) {
				current += child->computedSize.v[layoutAxis];
				currentMin += child->computedMinSize.v[layoutAxis];
				maxAxis = max(maxAxis, current);
				maxAxisMin = max(maxAxisMin, currentMin);
			}
			if (!(child->flags & BOX_FLAG_FLOATING_X << orthogonalAxis)) {
				maxOrthogonal = max(maxOrthogonal, child->computedSize.v[orthogonalAxis]);
				maxOrthogonalMin = max(maxOrthogonalMin, child->computedMinSize.v[orthogonalAxis]);
			}
		}
		box->computedSize.v[layoutAxis] = max(box->computedSize.v[layoutAxis], maxAxis);
		box->computedSize.v[orthogonalAxis] = max(box->computedSize.v[orthogonalAxis], maxOrthogonal);
		box->computedMinSize.v[layoutAxis] = max(box->computedMinSize.v[layoutAxis], maxAxisMin);
		box->computedMinSize.v[orthogonalAxis] = max(box->computedMinSize.v[orthogonalAxis], maxOrthogonalMin);
	}

	box->computedSize.x = max(box->computedSize.x, box->computedMinSize.x);
	box->computedSize.y = max(box->computedSize.y, box->computedMinSize.y);
}

void solve_layout_conflicts_and_position_boxes_recurse(Box* box, F32 scale) {
	V2F32 availableSpace = box->computedSize;
	Axis2 layoutAxis = box->layoutDirection == LAYOUT_DIRECTION_UP || box->layoutDirection == LAYOUT_DIRECTION_DOWN ? AXIS2_Y : AXIS2_X;
	Axis2 orthogonalAxis = layoutAxis == AXIS2_X ? AXIS2_Y : AXIS2_X;

	F32 currentLayoutOffset = 0.0F;
	F32 currentMin = 0.0F;
	V2F32 takenSpace{};
	V2F32 minTakenSpace{};
	for (Box* child = box->childFirst; child; child = child->next) {
		if (child->flags & BOX_FLAG_RESET_LAYOUT) {
			currentLayoutOffset = 0.0F;
			currentMin = 0.0F;
		}
		if (!(child->flags & BOX_FLAG_FLOATING_X << layoutAxis)) {
			currentLayoutOffset += child->computedSize.v[layoutAxis];
			currentMin += child->computedMinSize.v[layoutAxis];
			takenSpace.v[layoutAxis] = max(takenSpace.v[layoutAxis], currentLayoutOffset);
			minTakenSpace.v[layoutAxis] = max(minTakenSpace.v[layoutAxis], currentMin);
		}
		if (!(child->flags & BOX_FLAG_FLOATING_X << orthogonalAxis)) {
			takenSpace.v[orthogonalAxis] = max(takenSpace.v[orthogonalAxis], child->computedSize.v[orthogonalAxis]);
			minTakenSpace.v[orthogonalAxis] = max(minTakenSpace.v[orthogonalAxis], child->computedMinSize.v[orthogonalAxis]);
		}
	}

	// Fix the children so the (hopefully) fit
	V2F32 fixupBudget{ max(0.0F, takenSpace.x - minTakenSpace.x), max(0.0F, takenSpace.y - minTakenSpace.y) };
	V2F32 violation = takenSpace - availableSpace;
	F32 fixupRatio = violation.v[layoutAxis] / fixupBudget.v[layoutAxis];
	if ((violation.x > 0 || violation.y > 0) && (fixupBudget.x > 0 || fixupBudget.y > 0)) {
		for (Box* child = box->childFirst; child; child = child->next) {
			V2F32 childFixupBudget = child->computedSize - child->computedMinSize;
			if (violation.v[layoutAxis] > 0 && fixupBudget.v[layoutAxis] > 0 && !(child->flags & BOX_FLAG_FLOATING_X << layoutAxis)) {
				F32 fixupSizeLayoutAxis = childFixupBudget.v[layoutAxis] * fixupRatio;
				child->computedSize.v[layoutAxis] -= clamp(fixupSizeLayoutAxis, 0.0F, childFixupBudget.v[layoutAxis]);
			}
			if (violation.v[orthogonalAxis] > 0 && fixupBudget.v[orthogonalAxis] > 0 && !(child->flags & BOX_FLAG_FLOATING_X << orthogonalAxis)) {
				F32 fixupSizeOrthogonalAxis = child->computedSize.v[orthogonalAxis] - availableSpace.v[orthogonalAxis];
				child->computedSize.v[orthogonalAxis] -= clamp(fixupSizeOrthogonalAxis, 0.0F, childFixupBudget.v[orthogonalAxis]);
			}
		}
	}


	// Finalize the layout
	//TODO layout direction handling for up and left
	currentLayoutOffset = 0.0F;
	F32 availableOrthogonal = availableSpace.v[orthogonalAxis];
	for (Box* child = box->childFirst; child; child = child->next) {
		if (child->flags & BOX_FLAG_RESET_LAYOUT) {
			currentLayoutOffset = 0.0F;
		}
		if (!(child->flags & BOX_FLAG_FLOATING_X << layoutAxis)) {
			child->computedParentRelativeOffset.v[layoutAxis] = currentLayoutOffset;
			currentLayoutOffset += child->computedSize.v[layoutAxis];
		} else {
			child->computedParentRelativeOffset.v[layoutAxis] = 0.0F;
		}
		if (!(child->flags & BOX_FLAG_FLOATING_X << orthogonalAxis) && child->flags & BOX_FLAG_CENTER_ON_ORTHOGONAL_AXIS) {
			F32 childOrthogonal = child->computedSize.v[orthogonalAxis];
			child->computedParentRelativeOffset.v[orthogonalAxis] = max(availableOrthogonal * 0.5F - childOrthogonal * 0.5F, 0.0F);
		} else {
			child->computedParentRelativeOffset.v[orthogonalAxis] = 0.0F;
		}
	}

	for (Box* child = box->childFirst; child; child = child->next) {
		solve_layout_conflicts_and_position_boxes_recurse(child, scale * child->contentScale);
	}
}

void layout_boxes(U32 rootWidth, U32 rootHeight) {
	root.unsafeBox->minSize = root.unsafeBox->idealSize = root.unsafeBox->computedSize = V2F32{ F32(rootWidth), F32(rootHeight) };
	layout_boxes_recurse(root.unsafeBox);
	root.unsafeBox->computedSize = V2F32{ F32(rootWidth), F32(rootHeight) };
	solve_layout_conflicts_and_position_boxes_recurse(root.unsafeBox, root.unsafeBox->contentScale);
	root.unsafeBox->computedParentRelativeOffset = V2F32{};

	V2F32 contextBoxOffset{};
	for (U32 i = 0; i < contextMenuStack.size; i++) {
		if (Box* contextBox = contextMenuStack.data[i].get()) {
			layout_boxes_recurse(contextBox);
			solve_layout_conflicts_and_position_boxes_recurse(contextBox, contextBox->contentScale);
			contextBox->computedParentRelativeOffset = contextBoxOffset;
			contextBoxOffset = contextBox->contentOffset + contextBox->computedParentRelativeOffset + V2F32{ contextBox->computedSize.x, 0.0F };
			// Clamp the box inside the render area
			contextBox->computedParentRelativeOffset.x -= max(0.0F, contextBox->contentOffset.x + contextBox->computedParentRelativeOffset.x + contextBox->computedSize.x - rootWidth);
			contextBox->computedParentRelativeOffset.x -= min(0.0F, contextBox->contentOffset.x + contextBox->computedParentRelativeOffset.x);
			contextBox->computedParentRelativeOffset.y -= max(0.0F, contextBox->contentOffset.y + contextBox->computedParentRelativeOffset.y + contextBox->computedSize.y - rootHeight);
			contextBox->computedParentRelativeOffset.y -= min(0.0F, contextBox->contentOffset.y + contextBox->computedParentRelativeOffset.y);
		} else {
			for (U32 j = i; j < contextMenuStack.size; j++) {
				free_box(contextMenuStack.data[j]);
			}
			contextMenuStack.resize(i);
			break;
		}
	}

	if (Box* tooltipBox = tooltip.get()) {
		layout_boxes_recurse(tooltipBox);
		solve_layout_conflicts_and_position_boxes_recurse(tooltipBox, tooltipBox->contentScale);
		// Clamp the box inside the render area
		tooltipBox->computedParentRelativeOffset.x = -max(0.0F, tooltipBox->contentOffset.x + tooltipBox->computedSize.x - rootWidth);
		tooltipBox->computedParentRelativeOffset.x -= min(0.0F, tooltipBox->contentOffset.x + tooltipBox->computedParentRelativeOffset.x);
		tooltipBox->computedParentRelativeOffset.y = -max(0.0F, tooltipBox->contentOffset.y + tooltipBox->computedSize.y - rootHeight);
		tooltipBox->computedParentRelativeOffset.y -= min(0.0F, tooltipBox->contentOffset.y + tooltipBox->computedParentRelativeOffset.y);
	}
}

Rng2F32 compute_final_render_area_and_offset_for_children(V2F32* childrenOffset, Box* box, V2F32 parentComputedOffset, V2F32 offset, F32 scale) {
	*childrenOffset = V2F32{};
	V2F32 boxOffset{};
	// If floating, apply offset immediately, otherwise only apply it to children
	if (box->flags & BOX_FLAG_FLOATING_X) {
		boxOffset.x = box->contentOffset.x;
	} else {
		childrenOffset->x = box->contentOffset.x * scale;
	}
	if (box->flags & BOX_FLAG_FLOATING_Y) {
		boxOffset.y = box->contentOffset.y;
	} else {
		childrenOffset->y = box->contentOffset.y * scale;
	}
	V2F32 adjustedParentRelativeOffset = box->computedParentRelativeOffset + boxOffset;
	box->computedOffset = parentComputedOffset + adjustedParentRelativeOffset * scale;
	V2F32 finalStart = parentComputedOffset + adjustedParentRelativeOffset * scale + offset;
	V2F32 finalEnd = parentComputedOffset + (adjustedParentRelativeOffset + box->computedSize) * scale + offset;
	return Rng2F32{ finalStart.x, finalStart.y, finalEnd.x, finalEnd.y };
}
void draw_box(DynamicVertexBuffer::Tessellator& tes, Box* box, V2F32 mousePos, V2F32 parentComputedOffset, V2F32 offset, F32 scale, F32 z) {
	V2F32 childrenOffset;
	Rng2F32 renderArea = compute_final_render_area_and_offset_for_children(&childrenOffset, box, parentComputedOffset, offset, scale);
	if (box->flags & BOX_FLAG_CLIP_CHILDREN && currentClipBoxCount < MAX_CLIP_BOXES) {
		Rng2F32 boxRange = rng_intersect(renderArea, clipBoxStack.back());
		reinterpret_cast<Rng2F32*>(clipBoxBuffers[VK::currentFrameInFlight].mapping)[currentClipBoxCount] = boxRange;
		clipBoxIndexStack.push_back(currentClipBoxCount);
		clipBoxStack.push_back(boxRange);
		currentClipBoxCount++;
	}
	if (!(box->flags & BOX_FLAG_INVISIBLE)) {
		if (box->backgroundColor.a != 0) {
			V4F32 color = box->backgroundColor.to_v4f32();
			if (box->flags & BOX_FLAG_HIGHLIGHT_ON_USER_INTERACTION) {
				if (box == hotBox.get()) {
					color = V4F32{ min(1.0F, color.x + 0.1F), min(1.0F, color.y + 0.1F), min(1.0F, color.z + 0.1F) , color.w };
				}
				if (box == activeBox.get()) {
					color = RGBA8{ 71, 114, 179, 255 }.to_v4f32();
				}
			}
			Textures::Texture& tex = box->backgroundTexture ? *box->backgroundTexture : Textures::simpleWhite;
			U32 flags = clipBoxIndexStack.back() << 16;
			if (tex.type == Textures::TEXTURE_TYPE_MSDF) {
				flags |= VK::UI_RENDER_FLAG_MSDF;
			}
			tes.ui_rect2d(renderArea.minX, renderArea.minY, renderArea.maxX, renderArea.maxY, z, 0.0F, 0.0F, 1.0F, 1.0F, color, tex.index, flags);
		}
		if (!box->text.is_empty() && box->numTypedCharacters == 0) {
			V4F32 textColor = box->textColor.to_v4f32();
			if (box->typedTextBuffer && box->numTypedCharacters == 0) {
				textColor = V4F32{ textColor.x * 0.25F, textColor.y * 0.25F, textColor.z * 0.25F, textColor.w };
			}
			TextRenderer::draw_string_batched(tes, box->text, renderArea.minX + box->textBorderSpacing * scale, renderArea.minY + box->textBorderSpacing * scale, z, box->textSize * scale, textColor, clipBoxIndexStack.back() << 16);
		}
		if (box->numTypedCharacters) {
			TextRenderer::draw_string_batched(tes, StrA{ box->typedTextBuffer, box->numTypedCharacters }, renderArea.minX + box->textBorderSpacing * scale, renderArea.minY + box->textBorderSpacing * scale, z, box->textSize * scale, box->textColor.to_v4f32(), clipBoxIndexStack.back() << 16);
		}
	}
	for (Box* child = box->childLast; child; child = child->prev) {
		draw_box(tes, child, mousePos, offset + childrenOffset, box->computedOffset, scale * box->contentScale, z + child->zOffset);
	}
	if (box->flags & BOX_FLAG_CUSTOM_DRAW) {
		UserCommunication comm{};
		comm.mousePos = mousePos;
		comm.tessellator = &tes;
		comm.renderArea = renderArea;
		comm.scale = scale;
		comm.renderClipBox = clipBoxStack.back();
		comm.clipBoxIndex = clipBoxIndexStack.back();
		comm.renderZ = z;
		box->actionCallback(box, comm);
	}
	if (box->flags & BOX_FLAG_CLIP_CHILDREN && currentClipBoxCount <= MAX_CLIP_BOXES) {
		clipBoxIndexStack.pop_back();
		clipBoxStack.pop_back();
	}
}

void draw() {
	modificationLock.lock_read();
	DynamicVertexBuffer::Tessellator& tes = DynamicVertexBuffer::get_tessellator();
	tes.begin_draw(VK::uiPipeline, VK::uiPipelineLayout, DynamicVertexBuffer::DRAW_MODE_QUADS);
	tes.set_clip_boxes(clipBoxBuffers[VK::currentFrameInFlight].gpuAddress);
	Rng2F32 infRange = { -F32_LARGE, -F32_LARGE, F32_LARGE, F32_LARGE };
	reinterpret_cast<Rng2F32*>(clipBoxBuffers[VK::currentFrameInFlight].mapping)[0] = infRange;
	currentClipBoxCount = 1;
	clipBoxIndexStack.clear();
	clipBoxIndexStack.push_back(0);
	clipBoxStack.clear();
	clipBoxStack.push_back(infRange);
	V2F32 mousePos = Win32::get_mouse();
	draw_box(tes, root.unsafeBox, mousePos, V2F32{}, root.unsafeBox->contentOffset, root.unsafeBox->contentScale, root.unsafeBox->zOffset);
	for (BoxHandle contextMenuHandle : contextMenuStack) {
		if (Box* contextMenuBox = contextMenuHandle.get()) {
			draw_box(tes, contextMenuBox, mousePos, V2F32{}, V2F32{}, 1.0F, 2.0F);
		}
	}
	if (Box* tooltipBox = tooltip.get()) {
		draw_box(tes, tooltipBox, mousePos, V2F32{}, V2F32{}, 1.0F, 1.0F);
	}
	tes.end_draw();
	modificationLock.unlock_read();
}

B32 mouse_input_for_box_recurse(B32* anyContained, Box* box, V2F32 pos, Win32::MouseButton button, Win32::MouseValue state, V2F32 computedParentOffset, V2F32 offset, F32 scale) {
	V2F32 childrenOffset;
	Rng2F32 renderArea = compute_final_render_area_and_offset_for_children(&childrenOffset, box, computedParentOffset, offset, scale);
	B32 mouseOutside = !rng_contains_point(renderArea, pos);
	if (mouseOutside && box->flags & BOX_FLAG_CLIP_CHILDREN) {
		return false;
	}
	if (anyContained && !mouseOutside) {
		*anyContained = true;
	}
	for (Box* child = box->childFirst; child; child = child->next) {
		if (mouse_input_for_box_recurse(anyContained, child, pos, button, state, box->computedOffset, offset + childrenOffset, scale * box->contentScale)) {
			return true;
		}
	}
	if (mouseOutside || box->actionCallback == nullptr) {
		return false;
	}
	UserCommunication comm{};
	comm.mousePos = pos;
	comm.renderArea = renderArea;
	comm.scale = scale;
	if (button == Win32::MOUSE_BUTTON_WHEEL) {
		comm.scrollInput = state.scroll;
	} else {
		if (activeBox.get() == box) {
			if (state.state == Win32::BUTTON_STATE_UP) {
				comm.leftClicked = button == Win32::MOUSE_BUTTON_LEFT;
				comm.rightClicked = button == Win32::MOUSE_BUTTON_RIGHT;
				comm.middleClicked = button == Win32::MOUSE_BUTTON_MIDDLE;
				comm.mouse4Clicked = button == Win32::MOUSE_BUTTON_3;
				comm.mouse5Clicked = button == Win32::MOUSE_BUTTON_4;
			}
		} else if (Box* active = activeBox.get()) {
			if (active->actionCallback && state.state == Win32::BUTTON_STATE_UP) {
				comm.draggedTo = box;
				if (active->actionCallback(active, comm) == ACTION_HANDLED) {
					return true;
				}
				comm.draggedTo = nullptr;
			}
		} else if (hotBox.get() == box && state.state == Win32::BUTTON_STATE_DOWN) {
			comm.leftClickStart = button == Win32::MOUSE_BUTTON_LEFT;
			comm.rightClickStart = button == Win32::MOUSE_BUTTON_RIGHT;
			comm.middleClickStart = button == Win32::MOUSE_BUTTON_MIDDLE;
			comm.mouse4ClickStart = button == Win32::MOUSE_BUTTON_3;
			comm.mouse5ClickStart = button == Win32::MOUSE_BUTTON_4;
			activeBox = BoxHandle{ box, box->generation };
			activeTextBox = BoxHandle{};
			// This is a bit of a hack to avoid having to set parents while also having drag properly scaleed.
			// It won't update correctly if the user scales while dragging
			activeBoxTotalScale = scale;
		}
	}
	ActionResult result = box->actionCallback(box, comm);
	if (result == ACTION_HANDLED && !(box->flags & BOX_FLAG_DONT_CLOSE_CONTEXT_MENU_ON_INTERACTION)) {
		clear_context_menu();
	}
	return result == ACTION_HANDLED;
}
bool inDialog = false;
void handle_mouse_action(V2F32 pos, Win32::MouseButton button, Win32::MouseValue state) {
	if (inDialog) return;
	modificationLock.lock_write();
	for (I32 i = I32(contextMenuStack.size) - 1; i >= 0; i--) {
		if (Box* contextMenuBox = contextMenuStack.data[i].get()) {
			B32 anyContained = false;
			mouse_input_for_box_recurse(&anyContained, contextMenuBox, pos, button, state, V2F32{}, root.unsafeBox->contentOffset, root.unsafeBox->contentScale);
			if (anyContained) {
				goto contextMenuClicked;
			}
		}
	}
	if (button != Win32::MOUSE_BUTTON_WHEEL && state.state == Win32::BUTTON_STATE_DOWN) {
		context_menu(BoxHandle{}, BoxHandle{}, V2F32{});
	}
	mouse_input_for_box_recurse(nullptr, root.unsafeBox, pos, button, state, V2F32{}, root.unsafeBox->contentOffset, root.unsafeBox->contentScale);
contextMenuClicked:;
	if (button != Win32::MOUSE_BUTTON_WHEEL && state.state == Win32::BUTTON_STATE_UP) {
		activeBox = BoxHandle{};
	}
	modificationLock.unlock_write();
}
B32 mouse_update_for_box_recurse(B32* anyContains, Box* box, V2F32 pos, V2F32 delta, V2F32 computedParentOffset, V2F32 offset, F32 scale) {
	V2F32 childrenOffset;
	Rng2F32 renderArea = compute_final_render_area_and_offset_for_children(&childrenOffset, box, computedParentOffset, offset, scale);
	B32 mouseOutside = !rng_contains_point(renderArea, pos);
	if (mouseOutside && box->flags & BOX_FLAG_CLIP_CHILDREN) {
		return false;
	}
	if (anyContains && !mouseOutside) {
		*anyContains = true;
	}
	for (Box* child = box->childFirst; child; child = child->next) {
		if (mouse_update_for_box_recurse(anyContains, child, pos, delta, box->computedOffset, offset + childrenOffset, scale * box->contentScale)) {
			return true;
		}
	}
	if (mouseOutside || box->actionCallback == nullptr) {
		return false;
	}
	if (hotBox.get() != box) {
		hotBoxStartTimeSeconds = current_time_seconds();
	}
	hotBox = BoxHandle{ box, box->generation };
	Win32::set_cursor(box->hoverCursor);
	UserCommunication comm{};
	comm.mouseHovered = true;
	comm.mousePos = pos;
	comm.renderArea = renderArea;
	comm.scale = scale;
	box->actionCallback(box, comm);
	return true;
}

void handle_mouse_update(V2F32 pos, V2F32 delta) {
	modificationLock.lock_write();
	Box* active = activeBox.get();
	if (active && active->flags & BOX_FLAG_DISABLED) {
		activeBox = BoxHandle{};
		active = nullptr;
	}
	if (active && active->actionCallback) {
		UserCommunication comm{};
		comm.mousePos = pos;
		comm.drag = delta / activeBoxTotalScale;
		active->actionCallback(active, comm);
	}
	if (!active) {
		hotBox = BoxHandle{};
		for (I32 i = I32(contextMenuStack.size) - 1; i >= 0; i--) {
			if (Box* contextMenuBox = contextMenuStack.data[i].get()) {
				B32 anyContained = false;
				mouse_update_for_box_recurse(&anyContained, contextMenuBox, pos, delta, V2F32{}, V2F32{}, 1.0F);
				if (anyContained) {
					goto contextMenuHovered;
				}
			}
		}
		mouse_update_for_box_recurse(nullptr, root.unsafeBox, pos, delta, V2F32{}, root.unsafeBox->contentOffset, root.unsafeBox->contentScale);
	contextMenuHovered:;
		if (!hotBox.get() && rng_contains_point(Rng2F32{ 0.0F, 0.0F, F32(Win32::framebufferWidth), F32(Win32::framebufferHeight) }, pos)) {
			Win32::set_cursor(Win32::CURSOR_TYPE_POINTER);
		}
	}
	modificationLock.unlock_write();
}
B32 keyboard_input_for_box_recurse(B32* anyContained, Box* box, V2F32 pos, Win32::Key key, Win32::ButtonState state, V2F32 computedParentOffset, V2F32 offset, F32 scale) {
	V2F32 childrenOffset;
	Rng2F32 renderArea = compute_final_render_area_and_offset_for_children(&childrenOffset, box, computedParentOffset, offset, scale);
	B32 mouseOutside = !rng_contains_point(renderArea, pos);
	if (mouseOutside && box->flags & BOX_FLAG_CLIP_CHILDREN) {
		return false;
	}
	if (anyContained && !mouseOutside) {
		*anyContained = true;
	}
	for (Box* child = box->childFirst; child; child = child->next) {
		if (keyboard_input_for_box_recurse(anyContained, child, pos, key, state, box->computedOffset, offset + childrenOffset, scale * box->contentScale)) {
			return true;
		}
	}
	if (mouseOutside || box->actionCallback == nullptr || state != Win32::BUTTON_STATE_DOWN) {
		return false;
	}
	UserCommunication comm{};
	comm.mousePos = pos;
	comm.keyPressed = key;
	comm.charTyped = Win32::key_to_typed_char(key);

	ActionResult result = box->actionCallback(box, comm);
	return result == ACTION_HANDLED;
}
void handle_keyboard_action(V2F32 mousePos, Win32::Key key, Win32::ButtonState state) {
	modificationLock.lock_write();
	if (Box* activeTextInput = activeTextBox.get()) {
		if (state == Win32::BUTTON_STATE_DOWN) {
			UserCommunication comm{};
			comm.mousePos = mousePos;
			comm.keyPressed = key;
			comm.charTyped = Win32::key_to_typed_char(key);
			activeTextInput->actionCallback(activeTextInput, comm);
		}
	} else {
		for (I32 i = I32(contextMenuStack.size) - 1; i >= 0; i--) {
			if (Box* contextMenuBox = contextMenuStack.data[i].get()) {
				B32 anyContained = false;
				keyboard_input_for_box_recurse(&anyContained, contextMenuBox, mousePos, key, state, V2F32{}, V2F32{}, 1.0F);
				if (anyContained) {
					goto contextMenuTyped;
				}
			}
		}
		keyboard_input_for_box_recurse(nullptr, root.unsafeBox, mousePos, key, state, V2F32{}, root.unsafeBox->contentOffset, root.unsafeBox->contentScale);
	contextMenuTyped:;
	}
	modificationLock.unlock_write();
}



BoxHandle generic_box() {
	BoxHandle box = alloc_box();
	box.unsafeBox->parent = workingBox.unsafeBox;
	DLL_INSERT_TAIL(box.unsafeBox, workingBox.unsafeBox->childFirst, workingBox.unsafeBox->childLast, prev, next);
	return box;
}
void pop_box() {
	if (workingBox.unsafeBox != root.unsafeBox) {
		workingBox = BoxHandle{ workingBox.unsafeBox->parent, workingBox.unsafeBox->parent->generation };
	}
}
void layout_box(LayoutDirection direction, BoxActionCallback callback) {
	BoxHandle box = generic_box();
	box.unsafeBox->flags |= BOX_FLAG_INVISIBLE;
	box.unsafeBox->layoutDirection = direction;
	box.unsafeBox->actionCallback = callback;
	workingBox = box;
}
void ubox(BoxActionCallback callback = nullptr) {
	layout_box(LAYOUT_DIRECTION_UP, callback);
}
void dbox(BoxActionCallback callback = nullptr) {
	layout_box(LAYOUT_DIRECTION_DOWN, callback);
}
void lbox(BoxActionCallback callback = nullptr) {
	layout_box(LAYOUT_DIRECTION_LEFT, callback);
}
void rbox(BoxActionCallback callback = nullptr) {
	layout_box(LAYOUT_DIRECTION_RIGHT, callback);
}
#define UI_UBOX() DEFER_LOOP(UI::ubox(), UI::pop_box())
#define UI_DBOX() DEFER_LOOP(UI::dbox(), UI::pop_box())
#define UI_LBOX() DEFER_LOOP(UI::lbox(), UI::pop_box())
#define UI_RBOX() DEFER_LOOP(UI::rbox(), UI::pop_box())
BoxHandle spacer(F32 spacing) {
	BoxHandle box = generic_box();
	Axis2 layoutAxis = workingBox.unsafeBox->layoutDirection == LAYOUT_DIRECTION_UP || workingBox.unsafeBox->layoutDirection == LAYOUT_DIRECTION_DOWN ? AXIS2_Y : AXIS2_X;
	box.unsafeBox->idealSize = V2F32{};
	box.unsafeBox->idealSize.v[layoutAxis] = spacing;
	box.unsafeBox->minSize = V2F32{};
	box.unsafeBox->flags |= BOX_FLAG_INVISIBLE | BOX_FLAG_SPACER;
	return box;
}
BoxHandle spacer() {
	return spacer(BOX_INF_SIZE);
}
BoxHandle str_a(StrA str, BoxActionCallback actionCallback = nullptr) {
	BoxHandle box = generic_box();
	box.unsafeBox->text = str;
	box.unsafeBox->backgroundColor = RGBA8{};
	box.unsafeBox->actionCallback = actionCallback;
	return box;
}
BoxHandle text_input(StrA prompt, StrA defaultValue, BoxConsumer onTextUpdated = nullptr) {
	BoxHandle box = generic_box();
	box.unsafeBox->flags = BOX_FLAG_CLIP_CHILDREN | BOX_FLAG_HIGHLIGHT_ON_USER_INTERACTION;
	box.unsafeBox->sizeParentPercent.x = 1.0F;
	box.unsafeBox->minSize.x = 100.0F;
	box.unsafeBox->text = prompt;
	box.unsafeBox->typedTextBuffer = alloc_text_input();
	memcpy(box.unsafeBox->typedTextBuffer, defaultValue.str, defaultValue.length);
	box.unsafeBox->numTypedCharacters = U32(defaultValue.length);
	box.unsafeBox->userData[0] = UPtr(onTextUpdated);
	box.unsafeBox->actionCallback = [](Box* box, UserCommunication& comm) {
		if (comm.leftClicked) {
			activeTextBox = BoxHandle{ box, box->generation };
			return ACTION_HANDLED;
		}
		if (comm.keyPressed && activeTextBox.get() == box) {
			if (comm.charTyped && box->numTypedCharacters < MAX_TEXT_INPUT) {
				box->typedTextBuffer[box->numTypedCharacters++] = comm.charTyped;
			} else if (comm.keyPressed == Win32::KEY_BACKSPACE && box->numTypedCharacters > 0) {
				box->numTypedCharacters--;
			}
			BoxConsumer(box->userData[0])(box);
			return ACTION_HANDLED;
		}
		return ACTION_PASS;
	};
	return box;
}
BoxHandle button(Textures::Texture& tex, BoxConsumer onClick) {
	BoxHandle box = generic_box();
	box.unsafeBox->flags = BOX_FLAG_HIGHLIGHT_ON_USER_INTERACTION;
	box.unsafeBox->backgroundTexture = &tex;
	box.unsafeBox->hoverCursor = Win32::CURSOR_TYPE_HAND;
	box.unsafeBox->userData[0] = reinterpret_cast<UPtr>(onClick);
	if (onClick) {
		box.unsafeBox->actionCallback = [](Box* box, UserCommunication& com) {
			if (com.leftClicked) {
				reinterpret_cast<BoxConsumer>(box->userData[0])(box);
				return ACTION_HANDLED;
			}
			return ACTION_PASS;
		};
	}
	return box;
}
BoxHandle text_button(StrA text, BoxConsumer onClick) {
	BoxHandle box = generic_box();
	box.unsafeBox->flags = BOX_FLAG_HIGHLIGHT_ON_USER_INTERACTION;
	box.unsafeBox->text = text;
	box.unsafeBox->hoverCursor = Win32::CURSOR_TYPE_HAND;
	box.unsafeBox->userData[0] = reinterpret_cast<UPtr>(onClick);
	if (onClick) {
		box.unsafeBox->actionCallback = [](Box* box, UserCommunication& com) {
			if (com.leftClicked) {
				reinterpret_cast<BoxConsumer>(box->userData[0])(box);
				return ACTION_HANDLED;
			}
			return ACTION_PASS;
			};
	}
	return box;
}
void expandable_down(StrA name) {
	ubox([](Box* box, UserCommunication& com) { return ACTION_PASS; });
	UI_LBOX(); // clickable to expand
}
#define UI_EXPANDABLE_DOWN(name) DEFER_LOOP(UI::expandable_down(name), (UI::pop_box(), UI::pop_box()))
void dropdown() {
	UI_BACKGROUND_COLOR((V4F32{ 1.0F, 1.0F, 1.0F, 1.0F })) {
		UI_LBOX() {
			//...
		}
	}
}
#define UI_DROPDOWN() DEFER_LOOP(UI::dropdown(), UI::pop_box())

enum DropdownButtonFlagBits {
	DROPDOWN_BUTTON_FLAG_NONE = 0,
	DROPDOWN_BUTTON_FLAG_MAKE_DEFAULT_OPTION = (1 << 0)
};
typedef U32 DropdownButtonFlags;
BoxHandle dropdown_button(StrA name, DropdownButtonFlags flags = DROPDOWN_BUTTON_FLAG_NONE) {
	BoxHandle buttonBox = button(Textures::dropdownDown, nullptr);
	buttonBox.unsafeBox->idealSize = V2F32{ sizeStack.back().x, BOX_INF_SIZE };
	buttonBox.unsafeBox->minSize = sizeStack.back();
	return buttonBox;
}

BoxHandle toggle(Textures::Texture& texNotActivated, Textures::Texture& texActivated, BoxConsumer onToggled) {
	BoxHandle box = button(texNotActivated, onToggled);
	return box;
}
BoxHandle slider_number(F64 min, F64 max, F64 step, BoxConsumer onTextUpdated) {
	BoxHandle result;
	UI_RBOX() {
		UI_SIZE((V2F32{ 8.0F, 8.0F })) {
			BoxHandle incrementButton = button(Textures::uiIncrementLeft, [](Box* box) {
				Box* textInput = box->next;
				F64 num;
				StrA textStr = StrA{ textInput->typedTextBuffer, textInput->numTypedCharacters };
				if (SerializeTools::parse_f64(&num, &textStr) && SerializeTools::skip_whitespace(&textStr).is_empty()) {
					num += bitcast<F64>(box->userData[1]);
					textInput->numTypedCharacters = MAX_TEXT_INPUT;
					SerializeTools::serialize_f64(textInput->typedTextBuffer, &textInput->numTypedCharacters, roundf64(num * 10000.0) / 10000.0);
					if (box->userData[2]) {
						BoxConsumer(box->userData[2])(textInput);
					}
				}
			});
			incrementButton.unsafeBox->flags |= BOX_FLAG_CENTER_ON_ORTHOGONAL_AXIS;
			incrementButton.unsafeBox->userData[1] = bitcast<U64>(-step);
			incrementButton.unsafeBox->userData[2] = UPtr(onTextUpdated);
		}
		

		BoxHandle box = generic_box();
		result = box;
		box.unsafeBox->flags = BOX_FLAG_CLIP_CHILDREN | BOX_FLAG_HIGHLIGHT_ON_USER_INTERACTION;
		box.unsafeBox->sizeParentPercent.x = 1.0F;
		box.unsafeBox->text = ""sa;
		box.unsafeBox->typedTextBuffer = alloc_text_input();
		StrA defaultStr = "0.0"sa;
		memcpy(box.unsafeBox->typedTextBuffer, defaultStr.str, defaultStr.length);
		box.unsafeBox->numTypedCharacters = U32(defaultStr.length);
		box.unsafeBox->userData[0] = UPtr(onTextUpdated);
		box.unsafeBox->userData[1] = bitcast<U64>(step);
		box.unsafeBox->hoverCursor = Win32::CURSOR_TYPE_SIZE_HORIZONTAL;
		box.unsafeBox->actionCallback = [](Box* box, UserCommunication& comm) {
			if (comm.leftClicked) {
				activeTextBox = BoxHandle{ box, box->generation };
				return ACTION_HANDLED;
			}
			if (comm.keyPressed && activeTextBox.get() == box) {
				if (comm.charTyped && box->numTypedCharacters < MAX_TEXT_INPUT) {
					box->typedTextBuffer[box->numTypedCharacters++] = comm.charTyped;
				} else if (comm.keyPressed == Win32::KEY_BACKSPACE && box->numTypedCharacters > 0) {
					box->numTypedCharacters--;
				}
				if (box->userData[0]) {
					BoxConsumer(box->userData[0])(box);
				}
				return ACTION_HANDLED;
			}
			if (comm.drag.x) {
				F64 num;
				StrA textStr = StrA{ box->typedTextBuffer, box->numTypedCharacters };
				if (SerializeTools::parse_f64(&num, &textStr) && SerializeTools::skip_whitespace(&textStr).is_empty()) {
					num += bitcast<F64>(U64(box->userData[1])) * F64(comm.drag.x);
					box->numTypedCharacters = MAX_TEXT_INPUT;
					SerializeTools::serialize_f64(box->typedTextBuffer, &box->numTypedCharacters, roundf64(num * 10000.0) / 10000.0);
					if (box->userData[0]) {
						BoxConsumer(box->userData[0])(box);
					}
				}
				return ACTION_HANDLED;
			}
			return ACTION_PASS;
		};

		UI_SIZE((V2F32{ 8.0F, 8.0F })) {
			BoxHandle incrementButton = button(Textures::uiIncrementRight, [](Box* box) {
				Box* textInput = box->prev;
				F64 num;
				StrA textStr = StrA{ textInput->typedTextBuffer, textInput->numTypedCharacters };
				if (SerializeTools::parse_f64(&num, &textStr) && SerializeTools::skip_whitespace(&textStr).is_empty()) {
					num += bitcast<F64>(U64(box->userData[1]));
					textInput->numTypedCharacters = MAX_TEXT_INPUT;
					SerializeTools::serialize_f64(textInput->typedTextBuffer, &textInput->numTypedCharacters, roundf64(num * 10000.0) / 10000.0);
					if (box->userData[2]) {
						BoxConsumer(box->userData[2])(textInput);
					}
				}
			});
			incrementButton.unsafeBox->flags |= BOX_FLAG_CENTER_ON_ORTHOGONAL_AXIS;
			incrementButton.unsafeBox->userData[1] = bitcast<U64>(step);
			incrementButton.unsafeBox->userData[2] = UPtr(onTextUpdated);
		}
		
	}
	return result;
}

void scroll_bar(BoxHandle boxToScroll, F32 size) {
	UI_SIZE((V2F32{ size, size }))
	UI_DBOX() {
		button(Textures::uiIncrementUp, nullptr);

		BoxHandle spacerA = generic_box();
		spacerA.unsafeBox->minSize.y = 0.0F;
		spacerA.unsafeBox->sizeParentPercent.y = 0.0F;
		spacerA.unsafeBox->flags |= BOX_FLAG_INVISIBLE | BOX_FLAG_SPACER;

		BoxHandle scrollHandle = generic_box();
		scrollHandle.unsafeBox->flags |= BOX_FLAG_HIGHLIGHT_ON_USER_INTERACTION;
		scrollHandle.unsafeBox->backgroundColor = V4F32{ 0.2F, 0.2F, 0.2F, 1.0F }.to_rgba8();
		scrollHandle.unsafeBox->minSize.y = size * 4.0F;
		scrollHandle.unsafeBox->actionCallback = [](Box* box, UserCommunication& comm) {
			if (comm.drag.y) {
				BoxHandle toScroll;
				memcpy(&toScroll, box->userData, sizeof(BoxHandle));
				if (Box* scrollBox = toScroll.get()) {
					F32 maxScroll = max(scrollBox->childFirst->minSize.y - scrollBox->computedSize.y, 0.0F);
					F32 newScroll = clamp01((box->prev->computedSize.y + comm.drag.y) / (box->prev->computedSize.y + box->next->computedSize.y));
					scrollBox->contentOffset.y = -maxScroll * newScroll;
					box->prev->sizeParentPercent.y = newScroll;
					box->next->sizeParentPercent.y = 1.0F - newScroll;
				}
				return ACTION_HANDLED;
			}
			return ACTION_PASS;
		};
		memcpy(scrollHandle.unsafeBox->userData, &boxToScroll, sizeof(BoxHandle));

		BoxHandle spacerB = generic_box();
		spacerB.unsafeBox->minSize.y = 0.0F;
		spacerB.unsafeBox->sizeParentPercent.y = 1.0F;
		spacerB.unsafeBox->flags |= BOX_FLAG_INVISIBLE | BOX_FLAG_SPACER;

		button(Textures::uiIncrementDown, nullptr);
	}
}

BoxHandle context_menu_begin_helper() {
	BoxHandle dummyParent = alloc_box();
	dummyParent.unsafeBox->flags |= BOX_FLAG_INVISIBLE;
	workingBox = dummyParent;
	BoxHandle box = generic_box();
	workingBox = box;
	box.unsafeBox->layoutDirection = LAYOUT_DIRECTION_RIGHT;
	spacer(2.0F);
	dbox();
	return dummyParent;
}
void context_menu_end_helper(BoxHandle parent, V2F32 offset) {
	pop_box();
	spacer(2.0F);
	pop_box();
	context_menu(parent, workingBox, offset);
}

#define UI_ADD_CONTEXT_MENU(parent, offset) for (UI::BoxHandle oldWorkingBox = UI::workingBox, contextMenuBox = UI::context_menu_begin_helper(); oldWorkingBox.unsafeBox; UI::context_menu_end_helper(parent, offset), UI::workingBox = oldWorkingBox, oldWorkingBox.unsafeBox = nullptr)


}