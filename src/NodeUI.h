#pragma once
#include "DrillLib.h"
#include "Nodes.h"
#include "UI.h"
#include "Serialization.h"

namespace NodeUI {

struct Panel;
Panel* alloc_panel();
void free_panel(Panel* panel);

Panel* rootPanel;

const F32 PANEL_MIN_SIZE = 16.0F;
struct Panel {
	Panel* parent;
	Panel* childA;
	Panel* childB;

	Nodes::NodeGraph* nodeGraph;

	UI::BoxHandle uiBox;
	UI::BoxHandle content;

	void build_ui() {
		using namespace UI;
		Box* panelBox = uiBox.get();
		if (!panelBox) {
			return;
		}

		BoxHandle prevWorkingBox = workingBox;
		workingBox = BoxHandle{ panelBox, panelBox->generation };

		UI_RBOX() {
			UI_BACKGROUND_COLOR((V4F32{ 0.05F, 0.05F, 0.05F, 1.0F }))
			text_button("Save Project"sa, [](Box* box) {
				Nodes::NodeGraph* graph = reinterpret_cast<Panel*>(box->userData[1])->nodeGraph;
				if (graph) {
					char savePath[260] = {};
					OPENFILENAMEA fileDialogOptions{};
					fileDialogOptions.lStructSize = sizeof(fileDialogOptions);
					fileDialogOptions.hwndOwner = Win32::window;
					fileDialogOptions.lpstrFilter = "DAWdle Project (*.dawdle)\0*.dawdle\0";
					fileDialogOptions.lpstrFile = savePath;
					fileDialogOptions.nMaxFile = sizeof(savePath);
					fileDialogOptions.Flags = OFN_OVERWRITEPROMPT;
					inDialog = true;
					if (GetSaveFileNameA(&fileDialogOptions)) {
						Serialization::SaveNodeGraph(*graph, savePath);
					}
					inDialog = false;
				}
			}).unsafeBox->userData[1] = UPtr(this);
			spacer(500);
			UI_BACKGROUND_COLOR((V4F32{ 0.05F, 0.05F, 0.05F, 1.0F }))
			text_button("Load Project"sa, [](Box* box) {
				Nodes::NodeGraph* graph = reinterpret_cast<Panel*>(box->userData[1])->nodeGraph;
				if (graph) {
					char loadPath[260] = {};
					OPENFILENAMEA fileDialogOptions{};
					fileDialogOptions.lStructSize = sizeof(fileDialogOptions);
					fileDialogOptions.hwndOwner = Win32::window;
					fileDialogOptions.lpstrFilter = "DAWdle Project (*.dawdle)\0*.dawdle\0";
					fileDialogOptions.lpstrFile = loadPath;
					fileDialogOptions.nMaxFile = sizeof(loadPath);
					fileDialogOptions.Flags = OFN_FILEMUSTEXIST;
					inDialog = true;
					if (GetOpenFileNameA(&fileDialogOptions)) {
						Serialization::LoadNodeGraph(*graph, loadPath);
					}
					inDialog = false;
				}
			}).unsafeBox->userData[1] = UPtr(this);
			spacer();
			UI_BACKGROUND_COLOR((V4F32{ 0.02F, 0.02F, 0.02F, 1.0F }))
			button(Textures::uiX, [](Box* box) { reinterpret_cast<Panel*>(box->userData[1])->destroy(); }).unsafeBox->userData[1] = reinterpret_cast<UPtr>(this);
		}
		content = generic_box();
		Box* contentBox = content.unsafeBox;
		contentBox->idealSize = V2F32{ BOX_INF_SIZE, BOX_INF_SIZE };
		contentBox->backgroundColor = V4F32{ 0.1F, 0.1F, 0.1F, 1.0F }.to_rgba8();
		contentBox->flags = BOX_FLAG_CLIP_CHILDREN | BOX_FLAG_CUSTOM_DRAW;
		contentBox->hoverCursor = Win32::CURSOR_TYPE_SIZE_ALL;
		contentBox->actionCallback = [](UI::Box* box, UI::UserCommunication& comm) {
			Panel& panel = *reinterpret_cast<Panel*>(box->userData[0]);
			UI::ActionResult result = UI::ACTION_PASS;
			if (comm.tessellator) {
				panel.nodeGraph->draw_connections(*comm.tessellator, comm.renderZ, comm.clipBoxIndex);
				result = UI::ACTION_HANDLED;
			}
			if (comm.scrollInput != 0.0F) {
				V2F32 scaleCenter = comm.mousePos - box->computedOffset - box->contentOffset;
				F32 scaleFactor = 1.0F + clamp(comm.scrollInput * 0.001F, -0.2F, 0.2F);
				F32 newScale = clamp(box->contentScale * scaleFactor, 0.2F, 5.0F);
				F32 scaleFactorAfterClamp = newScale / box->contentScale;
				box->contentOffset += scaleCenter - scaleCenter * scaleFactorAfterClamp;
				box->contentScale = newScale;
				result = UI::ACTION_HANDLED;
			}
			if ((comm.drag.x != 0.0F || comm.drag.y != 0.0F) && box == UI::activeBox.get()) {
				box->contentOffset += comm.drag;
				result = UI::ACTION_HANDLED;
			}
			V2F32 mouseRelative = (comm.mousePos - box->computedOffset - box->contentOffset) / box->contentScale;
			if (comm.keyPressed == Win32::KEY_V) {
				panel.split(AXIS2_X);
				result = UI::ACTION_HANDLED;
			}
			if (comm.keyPressed == Win32::KEY_H) {
				panel.split(AXIS2_Y);
				result = UI::ACTION_HANDLED;
			}
			// Temporary, I don't have the necessary UI system to make a selection menu yet
			if (comm.keyPressed) {
				result = UI::ACTION_HANDLED;
			}
			if (comm.keyPressed == Win32::KEY_M) {
				panel.nodeGraph->create_node<Nodes::NodeMathOp>(mouseRelative);
			}
			if (comm.keyPressed == Win32::KEY_C) {
				panel.nodeGraph->create_node<Nodes::NodeChannelOut>(mouseRelative);
			}
			if (comm.keyPressed == Win32::KEY_T) {
				panel.nodeGraph->create_node<Nodes::NodeTimeIn>(mouseRelative);
			}
			if (comm.keyPressed == Win32::KEY_S) {
				panel.nodeGraph->create_node<Nodes::NodeSine>(mouseRelative);
			}
			if (comm.keyPressed == Win32::KEY_O) {
				panel.nodeGraph->create_node<Nodes::NodeOscilloscope>(mouseRelative);
			}
			if (comm.keyPressed == Win32::KEY_A) {
				panel.nodeGraph->create_node<Nodes::NodeSampler>(mouseRelative);
			}

			if (comm.rightClickStart) {
				UI_ADD_CONTEXT_MENU(BoxHandle{}, comm.mousePos) {
					workingBox.unsafeBox->userData[0] = UPtr(panel.nodeGraph);
					workingBox.unsafeBox->userData[1] = bitcast<U64>(mouseRelative);
					text_button("Math"sa, [](Box* box) {
						reinterpret_cast<Nodes::NodeGraph*>(box->parent->userData[0])->create_node<Nodes::NodeMathOp>(bitcast<V2F32>(box->parent->userData[1]));
					});
					text_button("Audio Out"sa, [](Box* box) {
						reinterpret_cast<Nodes::NodeGraph*>(box->parent->userData[0])->create_node<Nodes::NodeChannelOut>(bitcast<V2F32>(box->parent->userData[1]));
					});
					text_button("Time In"sa, [](Box* box) {
						reinterpret_cast<Nodes::NodeGraph*>(box->parent->userData[0])->create_node<Nodes::NodeTimeIn>(bitcast<V2F32>(box->parent->userData[1]));
					});
					text_button("Tone"sa, [](Box* box) {
						reinterpret_cast<Nodes::NodeGraph*>(box->parent->userData[0])->create_node<Nodes::NodeSine>(bitcast<V2F32>(box->parent->userData[1]));
					});
					text_button("Oscilloscope"sa, [](Box* box) {
						reinterpret_cast<Nodes::NodeGraph*>(box->parent->userData[0])->create_node<Nodes::NodeOscilloscope>(bitcast<V2F32>(box->parent->userData[1]));
					});
					text_button("Sampler"sa, [](Box* box) {
						reinterpret_cast<Nodes::NodeGraph*>(box->parent->userData[0])->create_node<Nodes::NodeSampler>(bitcast<V2F32>(box->parent->userData[1]));
					});
					BoxHandle test = generic_box();
					test.unsafeBox->flags |= BOX_FLAG_DONT_CLOSE_CONTEXT_MENU_ON_INTERACTION | BOX_FLAG_HIGHLIGHT_ON_USER_INTERACTION;
					test.unsafeBox->text = "Another context menu"sa;
					test.unsafeBox->userData[0] = UPtr(contextMenuBox.unsafeBox);
					test.unsafeBox->actionCallback = [](Box* box, UserCommunication& comm) {
						if (comm.leftClickStart) {
							BoxHandle parent{ reinterpret_cast<Box*>(box->userData[0]), reinterpret_cast<Box*>(box->userData[0])->generation };
							UI_ADD_CONTEXT_MENU(parent, (V2F32{ 0.0F, box->computedParentRelativeOffset.y })) {
								workingBox.unsafeBox->userData[0] = box->parent->userData[0];
								workingBox.unsafeBox->userData[1] = box->parent->userData[1];
								text_button("Sampler"sa, [](Box* box) {
									reinterpret_cast<Nodes::NodeGraph*>(box->parent->userData[0])->create_node<Nodes::NodeSampler>(bitcast<V2F32>(box->parent->userData[1]));
								});
							}
							return ACTION_HANDLED;
						}
						return ACTION_PASS;
					};
				}
			}
			return result;
		};
		content.unsafeBox->userData[0] = UPtr(this);

		if (nodeGraph) {
			if (UI::Box* graphBox = nodeGraph->uiBox.get()) {
				// It's important to not set graphBox's pointers here; unlike most boxes, it's expected to be children of multiple panels
				contentBox->childFirst = contentBox->childLast = graphBox;
			}
		}
		
		workingBox = prevWorkingBox;
	}

	B32 split(Axis2 axis) {
		Panel* newParent = alloc_panel();
		Panel* a = this;
		Panel* b = alloc_panel();
		newParent->uiBox = UI::alloc_box();
		newParent->uiBox.unsafeBox->flags = UI::BOX_FLAG_DONT_LAYOUT_TO_FIT_CHILDREN;
		newParent->uiBox.unsafeBox->sizeParentPercent = a->uiBox.unsafeBox->sizeParentPercent;
		b->uiBox = UI::alloc_box();
		b->uiBox.unsafeBox->flags = UI::BOX_FLAG_DONT_LAYOUT_TO_FIT_CHILDREN;
		b->uiBox.unsafeBox->sizeParentPercent = V2F32{ 1.0F, 1.0F };
		a->uiBox.unsafeBox->sizeParentPercent = V2F32{ 1.0F, 1.0F };

		DLL_REPLACE(uiBox.unsafeBox, newParent->uiBox.unsafeBox, uiBox.unsafeBox->parent->childFirst, uiBox.unsafeBox->parent->childLast, prev, next);

		if (parent) {
			*parent->child_ref(this) = newParent;
		} else {
			rootPanel = newParent;
		}

		newParent->uiBox.unsafeBox->parent = uiBox.unsafeBox->parent;
		newParent->parent = parent;
		a->parent = newParent;
		b->parent = newParent;
		newParent->childA = a;
		newParent->childB = b;
		
		a->uiBox.unsafeBox->parent = newParent->uiBox.unsafeBox;
		b->uiBox.unsafeBox->parent = newParent->uiBox.unsafeBox;

		b->nodeGraph = a->nodeGraph;

		b->build_ui();

		UI::BoxHandle draggableCenter = UI::alloc_box();
		draggableCenter.unsafeBox->parent = newParent->uiBox.unsafeBox;
		draggableCenter.unsafeBox->minSize.v[axis] = 3.0F;
		draggableCenter.unsafeBox->sizeParentPercent.v[axis2_orthogonal(axis)] = 1.0F;
		draggableCenter.unsafeBox->hoverCursor = axis == AXIS2_X ? Win32::CURSOR_TYPE_SIZE_HORIZONTAL : Win32::CURSOR_TYPE_SIZE_VERTICAL;
		draggableCenter.unsafeBox->backgroundColor = V4F32{ 0.7F, 0.7F, 0.7F, 1.0F }.to_rgba8();
		draggableCenter.unsafeBox->userData[0] = axis;
		draggableCenter.unsafeBox->actionCallback = [](UI::Box* box, UI::UserCommunication& com) {
			Axis2 splitAxis = Axis2(box->userData[0]);
			if (com.drag.v[splitAxis]) {
				F32 percentA = box->prev->sizeParentPercent.v[splitAxis];
				F32 percentB = box->next->sizeParentPercent.v[splitAxis];
				F32 normalizedDistance = percentA / (percentA + percentB);
				F32 parentRange = box->parent->computedSize.v[splitAxis];
				F32 currentSplitPos = parentRange * normalizedDistance;
				F32 nextSplitPos = clamp(currentSplitPos + com.drag.v[splitAxis], 0.0F, parentRange);
				box->prev->sizeParentPercent.v[splitAxis] = nextSplitPos / parentRange;
				box->next->sizeParentPercent.v[splitAxis] = 1.0F - box->prev->sizeParentPercent.v[splitAxis];
				return UI::ACTION_HANDLED;
			}
			return UI::ACTION_PASS;
		};
		UI::Box* parentBox = newParent->uiBox.unsafeBox;
		parentBox->layoutDirection = axis == AXIS2_X ? UI::LAYOUT_DIRECTION_RIGHT : UI::LAYOUT_DIRECTION_DOWN;
		DLL_INSERT_TAIL(a->uiBox.unsafeBox, parentBox->childFirst, parentBox->childLast, prev, next);
		DLL_INSERT_TAIL(draggableCenter.unsafeBox, parentBox->childFirst, parentBox->childLast, prev, next);
		DLL_INSERT_TAIL(b->uiBox.unsafeBox, parentBox->childFirst, parentBox->childLast, prev, next);
		return true;
	}

	void destroy() {
		if (!parent || childA || childB) {
			return;
		}
		UI::Box* parentBox = parent->uiBox.unsafeBox;
		UI::free_box(UI::BoxHandle{ parentBox->childFirst->next, parentBox->childFirst->next->generation });
		UI::free_box(uiBox);
		Panel* sibling = *parent->sibling_ref(this);
		UI::Box* siblingBox = sibling->uiBox.unsafeBox;
		siblingBox->sizeParentPercent = parentBox->sizeParentPercent;
		DLL_REMOVE(siblingBox, parentBox->childFirst, parentBox->childLast, prev, next);
		DLL_REPLACE(parentBox, siblingBox, parentBox->parent->childFirst, parentBox->parent->childLast, prev, next);
		siblingBox->parent = parentBox->parent;
		UI::free_box(parent->uiBox);
		if (parent->parent) {
			*parent->parent->child_ref(parent) = sibling;
		} else {
			rootPanel = sibling;
		}
		sibling->parent = parent->parent;
		free_panel(parent);
		free_panel(this);
	}

	Panel** sibling_ref(Panel* child) {
		return child == childA ? &childB :
			   child == childB ? &childA :
			   nullptr;
	}
	Panel** child_ref(Panel* child) {
		return child == childA ? &childA :
			   child == childB ? &childB :
			   nullptr;
	}
};

Panel* panelFreeList;

Panel* alloc_panel() {
	if (!panelFreeList) {
		panelFreeList = globalArena.alloc<Panel>(1);
		panelFreeList->childA = nullptr;
	}
	Panel* panel = panelFreeList;
	panelFreeList = panel->childA;
	*panel = Panel{};
	return panel;
}

void free_panel(Panel* panel) {
	panel->childA = panelFreeList;
	panelFreeList = panel;
}

void init(Nodes::NodeGraph* defaultGraph) {
	Panel* panel = alloc_panel();
	panel->nodeGraph = defaultGraph;
	panel->uiBox = UI::alloc_box();
	panel->uiBox.unsafeBox->flags = UI::BOX_FLAG_DONT_LAYOUT_TO_FIT_CHILDREN;
	panel->uiBox.unsafeBox->sizeParentPercent = V2F32{ 1.0F, 1.0F };
	rootPanel = panel;
	panel->build_ui();
	panel->uiBox.unsafeBox->parent = UI::root.unsafeBox;
	DLL_INSERT_HEAD(panel->uiBox.unsafeBox, UI::root.unsafeBox->childFirst, UI::root.unsafeBox->childLast, prev, next);
}

}