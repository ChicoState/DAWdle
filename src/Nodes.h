#pragma once
#include <commdlg.h>
#include <libsoundwave/AudioDecoder.h>
#include <filesystem>
#include "DrillLib.h"
#include "UI.h"
#include "ExpressionParser.h"

namespace Nodes {

const U32 PROCESS_BUFFER_SIZE = 1024;

const U32 INVALID_NODE_IDX = 0xFFFFFFFF;

// Random numbers
const U64 UI_TYPE_ID_NODE_WIDGET_INPUT = 0x1126F1C02281B4EEull;
const U64 UI_TYPE_ID_NODE_WIDGET_OUTPUT = 0xCE6949D284D6D8EDull;

struct NodeHeader;
void process_node(NodeHeader* node);

#define NODE_WIDGETS X(INPUT, NodeWidgetInput)\
	X(OUTPUT, NodeWidgetOutput)\
	X(OSCILLOSCOPE, NodeWidgetOscilloscope)\
	X(SAMPLER_BUTTON, NodeWidgetSamplerButton)\
	X(CUSTOM_UI_ELEMENT, NodeWidgetCustomUIElement)

#define NODES \
	X(TIME_IN, NodeTimeIn)\
	X(CHANNEL_OUT, NodeChannelOut)\
	X(WAVE, NodeWave)\
	X(MATH, NodeMathOp)\
	X(OSCILLOSCOPE, NodeOscilloscope)\
	X(SAMPLER, NodeSampler)\
	X(FILTER, NodeFilter)

#define X(enumName, typeName) NODE_##enumName,
enum NodeType : U32 {
	NODES
	NODE_COUNT
};
#undef X

#define X(enumName, typeName) NODE_WIDGET_##enumName,
enum NodeWidgetType : U32 {
	NODE_WIDGETS
	NODE_WIDGET_Count
};
#undef X
enum NodeIOType : U32 {
	NODE_IO_SCALAR, // Single value, unchanging
	NODE_IO_BUFFER, // Buffer of values, different one for each sample
	NODE_IO_LIST_BUFFER, // List of buffers (typically small buffers, used for things like multiple notes playing at a time). Comes with a buffer of values plus a buffer of list offsets for each sample
	NODE_IO_COUNT
};
struct NodeIOValue {
	F64 scalarBuffer[8];
	F64* buffer;
	U32* listEnds;
	U32 bufferLength;
	U32 listEndsLength;
	U32 bufferMask;

	void set_scalar(F64 val) {
		for (U32 i = 0; i < ARRAY_COUNT(scalarBuffer); i++) {
			scalarBuffer[i] = val;
		}
		buffer = scalarBuffer;
		listEnds = nullptr;
		bufferLength = 1;
		listEndsLength = 0;
		bufferMask = ARRAY_COUNT(scalarBuffer) - 1;
	}
};
void make_node_io_consistent(NodeIOValue** inputs, U32 inputCount, NodeIOValue** outputs, U32 outputCount) {
	if (inputCount == 0) {
		for (U32 i = 0; i < outputCount; i++) {
			*outputs[i] = NodeIOValue{};
		}
		return;
	}

	U32 bufferLength = U32_MAX;
	U32 listEndsLength = U32_MAX;
	U32* listEnds = nullptr;
	B32 hasDifferentListEnds = false;
	B32 allScalar = true;
	for (U32 i = 0; i < inputCount; i++) {
		NodeIOValue& input = *inputs[i];
		// All buffers *should* be the same size, but just in case
		if (input.buffer != input.scalarBuffer) {
			bufferLength = min(bufferLength, input.bufferLength);
			allScalar = false;
		}
		listEndsLength = min(listEndsLength, input.listEndsLength);
		if (input.listEndsLength) {
			if (listEnds != nullptr && listEnds != input.listEnds) {
				hasDifferentListEnds = true;
			}
			listEnds = input.listEnds;
		}
	}
	if (allScalar) {
		bufferLength = 1;
	}
	listEndsLength = min(listEndsLength, bufferLength);

	if (listEnds) {
		// This path should be a rarer case, I'm not going to bother optimizing it

		if (hasDifferentListEnds) {
			bufferLength = 0;
			listEnds = audioArena.alloc_aligned_with_slack<U32>(listEndsLength, alignof(__m256), 2 * sizeof(__m256));
			for (U32 i = 0; i < listEndsLength; i++) {
				U32 maxElementSize = 0;
				for (U32 j = 0; j < inputCount; j++) {
					if (inputs[j]->listEnds) {
						maxElementSize = max(maxElementSize, inputs[j]->listEnds[i]);
					}
				}
				bufferLength += maxElementSize;
				listEnds[i] = bufferLength;
			}
		}
		
		for (U32 i = 0; i < inputCount; i++) {
			NodeIOValue& input = *inputs[i];
			if (input.buffer == input.scalarBuffer) {
				continue;
			}
			U32* oldListEnds = input.listEnds;
			F64* oldData = input.buffer;
			input.listEndsLength = listEndsLength;
			input.listEnds = listEnds;
			input.bufferLength = bufferLength;
			if (oldListEnds && hasDifferentListEnds) {
				F64* newData = audioArena.alloc_aligned_with_slack<F64>(bufferLength, alignof(__m256), 2 * sizeof(__m256));
				input.buffer = newData;
				U32 newOffset = 0;
				U32 oldOffset = 0;
				for (U32 j = 0; j < listEndsLength; j++) {
					U32 newSize = listEnds[j] - newOffset;
					U32 oldSize = oldListEnds[j] = oldOffset;
					for (U32 k = 0; k < newSize; k++) {
						newData[newOffset + k] = k < oldSize ? oldData[oldOffset + k] : 0.0F;
					}
					newOffset = listEnds[j];
					oldOffset = oldListEnds[j];
				}
			} else if (!oldListEnds) {
				F64* newData = audioArena.alloc_aligned_with_slack<F64>(bufferLength, alignof(__m256), 2 * sizeof(__m256));
				input.buffer = newData;
				U32 newOffset = 0;
				for (U32 j = 0; j < listEndsLength; j++) {
					U32 newSize = listEnds[j] - newOffset;
					for (U32 k = 0; k < newSize; k++) {
						newData[newOffset + k] = oldData[j];
					}
					newOffset = listEnds[j];
				}
			} else {
				// This is a buffer list and is compatible with all other buffer lists (doesn't need changing)
			}
		}
	}
	for (U32 i = 0; i < outputCount; i++) {
		NodeIOValue& output = *outputs[i];
		output.bufferLength = bufferLength;
		output.listEndsLength = listEndsLength;
		output.listEnds = listEnds;
		if (bufferLength <= ARRAY_COUNT(output.scalarBuffer)) {
			output.bufferMask = ARRAY_COUNT(output.scalarBuffer) - 1;
			output.buffer = output.scalarBuffer;
		} else {
			output.bufferMask = U32_MAX;
			output.buffer = audioArena.alloc_aligned_with_slack<F64>(bufferLength, alignof(__m256), 2 * sizeof(__m256));
		}
	}
}

struct NodeHeader;

struct NodeWidgetHeader {
	NodeWidgetType type;
	U64 generation;
	NodeHeader* parent;
	NodeWidgetHeader* next;
	NodeWidgetHeader* prev;

	void init(NodeWidgetType widgetType) {
		type = widgetType;
		next = prev = nullptr;
	}
};
template<typename Widget>
struct NodeWidgetHandle {
	Widget* unsafeWidget;
	U64 generation;

	FINLINE Widget* get() {
		return !unsafeWidget || unsafeWidget->header.generation == 0 || unsafeWidget->header.generation > generation ? nullptr : unsafeWidget;
	}
};
struct NodeWidgetOutput {
	NodeWidgetHeader header;
	NodeIOValue value;

	UI::BoxHandle uiBoxConnector;

	V2F32 connectionRenderPos;

	void init() {
		header.init(NODE_WIDGET_OUTPUT);
		value = NodeIOValue{};
	}

	void add_to_ui();

	void destroy() {

	}
};

struct NodeWidgetInput {
	NodeWidgetHeader header;
	NodeWidgetHandle<NodeWidgetOutput> inputHandle;
	NodeIOValue value;
	F64 defaultValue;
	UI::BoxHandle sliderHandle;

	V2F32 connectionRenderPos;

	tbrs::ByteProgram program;

	void init(F64 defaultVal) {
		header.init(NODE_WIDGET_INPUT);
		defaultValue = defaultVal;
		inputHandle = NodeWidgetHandle<NodeWidgetOutput>{};
		program.init();
	}

	void connect(NodeWidgetOutput* outputWidget) {
		inputHandle = NodeWidgetHandle<NodeWidgetOutput>{ outputWidget, outputWidget->header.generation };
	}

	void add_to_ui() {
		using namespace UI;
		UI_RBOX() {
			workingBox.unsafeBox->backgroundColor = V4F32{ 0.05F, 0.05F, 0.05F, 1.0F }.to_rgba8();
			workingBox.unsafeBox->flags &= ~BOX_FLAG_INVISIBLE;
			spacer(6.0F);

			UI_BACKGROUND_COLOR((V4F32{ 0.1F, 0.2F, 0.1F, 1.0F }))
			sliderHandle = slider_number(-F64_INF, F64_INF, 0.1, [](Box* box) {
				NodeWidgetInput& input = *reinterpret_cast<NodeWidgetInput*>(box->userData[3]);
					StrA parseStr{ box->typedTextBuffer, box->numTypedCharacters };
					tbrs::parse_program(&input.program, parseStr);
			});
			sliderHandle .unsafeBox->userData[3] = UPtr(this);
			UI_SIZE((V2F32{ 8.0F, 8.0F })) {
				Box* connector = generic_box().unsafeBox;
				connector->flags = BOX_FLAG_FLOATING_X | BOX_FLAG_CUSTOM_DRAW | BOX_FLAG_CENTER_ON_ORTHOGONAL_AXIS;
				connector->contentOffset.x = -4.0F;
				connector->backgroundTexture = &Textures::nodeConnect;
				connector->userTypeId = UI_TYPE_ID_NODE_WIDGET_INPUT;
				connector->userData[0] = UPtr(this);
				connector->actionCallback = [](Box* box, UserCommunication& comm) {
					NodeWidgetInput& inputWidget = *reinterpret_cast<NodeWidgetInput*>(box->userData[0]);
					ActionResult result = ACTION_PASS;
					if (box == UI::activeBox.get() && comm.tessellator) {
						F32 handleScale = distance(comm.renderArea.midpoint(), comm.mousePos) * 0.5F;
						comm.tessellator->ui_bezier_curve(comm.renderArea.midpoint(), comm.renderArea.midpoint() - V2F32{ handleScale, 0.0F }, comm.mousePos + V2F32{ handleScale, 0.0F }, comm.mousePos, comm.renderZ + 0.05F, 32, 2.0F, V4F32{ 1.0F, 1.0F, 1.0F, 1.0F }, Textures::simpleWhite.index, comm.clipBoxIndex << 16);
						result = ACTION_HANDLED;
					}
					if (comm.tessellator) {
						inputWidget.connectionRenderPos = comm.renderArea.midpoint();
					}
					if (comm.draggedTo && comm.draggedTo->userTypeId == UI_TYPE_ID_NODE_WIDGET_OUTPUT) {
						inputWidget.connect(reinterpret_cast<NodeWidgetOutput*>(comm.draggedTo->userData[0]));
						result = ACTION_HANDLED;
					}
					if (comm.leftClickStart && inputWidget.inputHandle.get()) {
						BoxHandle otherConnector = inputWidget.inputHandle.get()->uiBoxConnector;
						inputWidget.inputHandle = {};
						if (otherConnector.get()) {
							UI::activeBox = otherConnector;
							result = ACTION_HANDLED;
						}
					}
					return ACTION_PASS;
				};
			}
		}
	}

	void destroy() {
		program.destroy();
	}
};
void NodeWidgetOutput::add_to_ui() {
	using namespace UI;
	UI_RBOX() {
		workingBox.unsafeBox->backgroundColor = V4F32{ 0.05F, 0.05F, 0.05F, 1.0F }.to_rgba8();
		workingBox.unsafeBox->flags &= ~BOX_FLAG_INVISIBLE;
		spacer();
		str_a("Output"sa);
		spacer();
		UI_SIZE((V2F32{ 8.0F, 8.0F })) {
			Box* connector = generic_box().unsafeBox;
			connector->flags = BOX_FLAG_FLOATING_X | BOX_FLAG_CUSTOM_DRAW | BOX_FLAG_CENTER_ON_ORTHOGONAL_AXIS;
			connector->contentOffset.x = 120.0F - 4.0F;
			connector->backgroundTexture = &Textures::nodeConnect;
			connector->userTypeId = UI_TYPE_ID_NODE_WIDGET_OUTPUT;
			connector->userData[0] = UPtr(this);
			connector->actionCallback = [](Box* box, UserCommunication& comm) {
				NodeWidgetOutput& outputWidget = *reinterpret_cast<NodeWidgetOutput*>(box->userData[0]);
				ActionResult result = ACTION_PASS;
				if (box == UI::activeBox.get() && comm.tessellator) {
					F32 handleScale = distance(comm.renderArea.midpoint(), comm.mousePos) * 0.5F;
					comm.tessellator->ui_bezier_curve(comm.renderArea.midpoint(), comm.renderArea.midpoint() + V2F32{ handleScale, 0.0F }, comm.mousePos - V2F32{ handleScale, 0.0F }, comm.mousePos, comm.renderZ + 0.05F, 32, 2.0F, V4F32{ 1.0F, 1.0F, 1.0F, 1.0F }, Textures::simpleWhite.index, comm.clipBoxIndex << 16);
					result = ACTION_HANDLED;
				}
				if (comm.tessellator) {
					outputWidget.connectionRenderPos = comm.renderArea.midpoint();
				}
				if (comm.draggedTo && comm.draggedTo->userTypeId == UI_TYPE_ID_NODE_WIDGET_INPUT) {
					reinterpret_cast<NodeWidgetInput*>(comm.draggedTo->userData[0])->connect(&outputWidget);
					result = ACTION_HANDLED;
				}
				return result;
			};
			uiBoxConnector = BoxHandle{ connector, connector->generation };
		}
	}
}
struct NodeWidgetOscilloscope {
	NodeWidgetHeader header;
	F32* waveformBuffer;
	U32 sampleRate;
	size_t waveformBufferSize;

	void init() {
		header.init(NODE_WIDGET_OSCILLOSCOPE);
		waveformBufferSize = PROCESS_BUFFER_SIZE;
		waveformBuffer = new F32[waveformBufferSize];
		std::fill_n(waveformBuffer, waveformBufferSize, 0.0f);
		sampleRate = 44100;
	}

	void add_to_ui() {
		using namespace UI;
		UI_RBOX() {
			workingBox.unsafeBox->backgroundColor = V4F32{ 0.1F, 0.1F, 0.1F, 0.9F }.to_rgba8();
			workingBox.unsafeBox->flags &= ~BOX_FLAG_INVISIBLE;
			BoxHandle contentBox = generic_box();
			contentBox.unsafeBox->backgroundColor = V4F32{ 0.0F, 0.0F, 0.0F, 1.0F }.to_rgba8();
			contentBox.unsafeBox->flags |= BOX_FLAG_CUSTOM_DRAW;
			contentBox.unsafeBox->minSize = V2F32{ 200.0F, 100.0F };
			contentBox.unsafeBox->actionCallback = [](Box* box, UserCommunication& comm) {
				NodeWidgetOscilloscope& osc = *reinterpret_cast<NodeWidgetOscilloscope*>(box->userData[1]);
				if (comm.tessellator) {
					V2F32 origin = box->computedOffset + box->parent->computedOffset;
					F32 width = comm.renderArea.maxX - comm.renderArea.minX;
					F32 height = comm.renderArea.maxY - comm.renderArea.minY;
					MemoryArena& arena = get_scratch_arena();
					MEMORY_ARENA_FRAME(arena) {
						V2F32* points = arena.alloc<V2F32>(osc.waveformBufferSize);
						//TODO the number of points can be reduced depending on the render size (no point in rendering a 1024 segment line if it's only 100 pixels wide)
						for (size_t i = 0; i < osc.waveformBufferSize; ++i) {
							points[i].x = origin.x + (i / (F32)osc.waveformBufferSize) * width;
							points[i].y = origin.y + height / 2 + osc.waveformBuffer[i] * height / 2;
						}
						comm.tessellator->ui_line_strip(points, osc.waveformBufferSize, comm.renderZ, 2.0F, V4F32{ 1.0F, 1.0F, 1.0F, 1.0F }, Textures::simpleWhite.index, comm.clipBoxIndex << 16);
					}
				}
				return UI::ACTION_HANDLED;
				};
			contentBox.unsafeBox->userData[1] = UPtr(this);
			spacer(20.0F);
		}
	}

	void updateWaveform(const double* data, U32 size) {
		size_t copySize = std::min(static_cast<size_t>(size), waveformBufferSize);
		for (size_t i = 0; i < copySize; ++i) {
			waveformBuffer[i] = static_cast<float>(data[i]);
		}
	}

	void destroy() {
		delete[] waveformBuffer;
	}
};
struct NodeWidgetSamplerButton {
	NodeWidgetHeader header;
	char path[512];
	F32* audioData = nullptr;
	U64 numSamples = 0;
	I32 sampleRate = 0;

	void init() {
		header.init(NODE_WIDGET_SAMPLER_BUTTON);
	}

	void add_to_ui() {
		using namespace UI;
		UI_RBOX() {
			workingBox.unsafeBox->backgroundColor = V4F32{ 0.05F, 0.05F, 0.05F, 1.0F }.to_rgba8();
			workingBox.unsafeBox->flags &= ~BOX_FLAG_INVISIBLE;
			spacer(20.0F);
			UI_BACKGROUND_COLOR((V4F32{ 0.1F, 0.1F, 0.1F, 0.0F }))
				text_button("Load Sample"sa, [](Box* box) {
					NodeWidgetSamplerButton& button = *reinterpret_cast<NodeWidgetSamplerButton*>(box->userData[1]);

					OPENFILENAMEA fileDialogOptions{};
					fileDialogOptions.lStructSize = sizeof(fileDialogOptions);
					fileDialogOptions.hwndOwner = Win32::window;
					fileDialogOptions.hInstance = Win32::instance;
					fileDialogOptions.lpstrFilter = "Sound Files (*.flac; *.ogg; *.opus; *.wav)\0*.flac;*.ogg;*.opus;*.wav\0\0";
					fileDialogOptions.lpstrFile = button.path;
					fileDialogOptions.nMaxFile = sizeof(button.path);
					inDialog = true;
					GetOpenFileNameA(&fileDialogOptions);
					inDialog = false;

					button.loadFromFile();
				}).unsafeBox->userData[1] = UPtr(this);
			spacer(20.0F);
		}
	}

	void loadFromFile() {
		if (!std::filesystem::exists(path)) return;

		soundwave::SoundwaveIO loader;
		auto data = std::make_unique<soundwave::AudioData>();
		loader.Load(data.get(), path);

		if (data->channelCount == 2) {
			numSamples = data->samples.size() / 2;
			audioData = new F32[numSamples];
			soundwave::StereoToMono(data->samples.data(), audioData, data->samples.size());
		}
		else {
			numSamples = data->samples.size();
			audioData = new F32[numSamples];
			memcpy(audioData, data->samples.data(), data->samples.size() * sizeof(F32));
		}
		sampleRate = data->sampleRate;
	}

	void destroy() {
		delete audioData;
	}
};
struct NodeWidgetCustomUIElement {
	NodeWidgetHeader header;
	UI::BoxHandle uiBox;
	void init(UI::BoxHandle box) {
		header.init(NODE_WIDGET_CUSTOM_UI_ELEMENT);
		uiBox = box;
	}
	void add_to_ui() {
		if (UI::Box* box = uiBox.get()) {
			box->parent = UI::workingBox.unsafeBox;
			DLL_INSERT_TAIL(box, UI::workingBox.unsafeBox->childFirst, UI::workingBox.unsafeBox->childLast, prev, next);
		}
	}
	void destroy() {};
};
union NodeWidget {
	NodeWidgetHeader header;
	NodeWidgetInput input;
	NodeWidgetOutput output;
	NodeWidgetOscilloscope oscilloscope;
	NodeWidgetSamplerButton file_dialog_button;
	NodeWidgetCustomUIElement customUIElement;
};

struct NodeGraph;
union Node;
Node* alloc_node();
void free_node(Node* toFree);

NodeWidgetHeader* freeWidgetListHead = nullptr;
U64 currentWidgetGeneration = 1;
NodeWidget* alloc_widget() {
	if (!freeWidgetListHead) {
		const U32 amountToAlloc = 256;
		NodeWidget* block = globalArena.alloc<NodeWidget>(amountToAlloc);
		for (U32 i = 0; i < amountToAlloc; i++) {
			block[i].header.next = &block[i + 1].header;
		}
		block[amountToAlloc - 1].header.next = nullptr;
		freeWidgetListHead = &block->header;
	}
	NodeWidgetHeader* allocated = freeWidgetListHead;
	freeWidgetListHead = allocated->next;
	return reinterpret_cast<NodeWidget*>(allocated);
}
void free_widget(NodeWidget* toFree) {
	toFree->header.next = freeWidgetListHead;
	toFree->header.generation = 0;
	freeWidgetListHead = &toFree->header;
}

struct NodeHeader {
	NodeType type;
	static constexpr U32 TITLE_CAPACITY = 32;
	U32 titleLength;
	char title[TITLE_CAPACITY];
	V2F32 offset;
	B32 hasProcessed;
	NodeWidgetHeader* widgetBegin;
	NodeWidgetHeader* widgetEnd;
	U32 serializeIndex;

	NodeGraph* parent;
	NodeHeader* prev;
	NodeHeader* next;
	NodeHeader* selectedPrev;
	NodeHeader* selectedNext;
	UI::BoxHandle uiBox;
	UI::BoxHandle uiNodeTitleBox;

	void init(NodeType nodeType, StrA straTitle) {
		type = nodeType;
		titleLength = U32(straTitle.length);
		ASSERT(titleLength < TITLE_CAPACITY, "title length too long");
		memcpy(title, straTitle.str, straTitle.length);
		offset = V2F32{};
		hasProcessed = false;
		widgetEnd = widgetBegin = nullptr;
		prev = next = nullptr;
		selectedPrev = selectedNext = nullptr;
		uiBox = UI::BoxHandle{};
		serializeIndex = 0;
	}

	void destroy() {
		UI::free_box(uiBox);
		for (NodeWidgetHeader* widget = widgetBegin; widget != nullptr;) {
			NodeWidgetHeader* nextWidget = widget->next;
#define X(enumName, typeName) case NODE_WIDGET_##enumName: reinterpret_cast<typeName*>(widget)->destroy(); break;
			switch (widget->type) {
				NODE_WIDGETS
			default: break;
			}
#undef X
			free_widget(reinterpret_cast<NodeWidget*>(widget));
			widget = nextWidget;
		}
		free_node(reinterpret_cast<Node*>(this));
	}

	NodeWidget* add_widget() {
		NodeWidget* widget = alloc_widget();
		widget->header = NodeWidgetHeader{};
		widget->header.generation = currentWidgetGeneration++;
		widget->header.parent = this;
		if (widgetBegin == nullptr) {
			// Accessing the header through a union like this and then accessing it through a widget member is UB, but MSVC is so unlikely to break it that I don't care
			widgetBegin = widgetEnd = &widget->header;
		} else {
			widget->header.prev = widgetEnd;
			widgetEnd->next = &widget->header;
			widgetEnd = &widget->header;
		}
		return widget;
	}

	NodeWidgetHeader* get_nth_of_type(NodeWidgetType widgetType, U32 idx) {
		U32 outputIdx = 0;
		for (NodeWidgetHeader* widget = widgetBegin; widget != nullptr; widget = widget->next) {
			if (widget->type == widgetType) {
				if (outputIdx == idx) {
					return widget;
				}
				outputIdx++;
			}
		}
		return nullptr;
	}
	NodeWidgetOutput* get_output(U32 idx) {
		return reinterpret_cast<NodeWidgetOutput*>(get_nth_of_type(NODE_WIDGET_OUTPUT, idx));
	}
	NodeWidgetInput* get_input(U32 idx) {
		return reinterpret_cast<NodeWidgetInput*>(get_nth_of_type(NODE_WIDGET_INPUT, idx));
	}
	NodeWidgetSamplerButton* get_samplerbutton(U32 idx) {
		return reinterpret_cast<NodeWidgetSamplerButton*>(get_nth_of_type(NODE_WIDGET_SAMPLER_BUTTON, idx));
	}

	UI::Box* add_to_ui();
};
struct NodeTimeIn {
	NodeHeader header;
	void init() {
		header.init(NODE_TIME_IN, "Time"sa);
		header.add_widget()->output.init();
	}
	void process();
	void add_to_ui() {
		using namespace UI;
		Box* box = header.add_to_ui();
	}
};
struct NodeChannelOut {
	NodeHeader header;
	NodeChannelOut* outputPrev;
	NodeChannelOut* outputNext;
	

	void init() {
		header.init(NODE_CHANNEL_OUT, "Channel Out"sa);
		header.add_widget()->input.init(0.0);
	}
	void process() {
		
	}
	F64* get_output_buffer(U32* lengthOut, U32* maskOut) {
		NodeIOValue& val = header.get_input(0)->value;
		if (val.listEndsLength) {
			return nullptr;
		} else {
			*lengthOut = val.bufferLength;
			*maskOut = val.bufferMask;
			return val.buffer;
		}
	}
	void add_to_ui() {
		using namespace UI;
		Box* box = header.add_to_ui();
	}
};

__m256i lcg_a = _mm256_set1_epi32(1664525);
__m256i lcg_c = _mm256_set1_epi32(1013904223);
__m256i lcg_seed = _mm256_set1_epi32(12345);
__m256 random_f32() {
	lcg_seed = _mm256_add_epi32(_mm256_mullo_epi32(lcg_seed, lcg_a), lcg_c);
	__m256 float_0_to_1 = _mm256_cvtepi32_ps(_mm256_and_si256(lcg_seed, _mm256_set1_epi32(0x7FFFFFFF)));
	float_0_to_1 = _mm256_mul_ps(float_0_to_1, _mm256_set1_ps(1.0f / 0x7FFFFFFF));
	return _mm256_sub_ps(_mm256_mul_ps(float_0_to_1, _mm256_set1_ps(2.0f)), _mm256_set1_ps(1.0f));
}

enum Waveform {
	WAVE_SINE,
	WAVE_SAWTOOTH,
	WAVE_SQUARE,
	WAVE_TRIANGLE,
	WAVE_NOISE
};

StrA waveform_name(Waveform w) {
	switch (w) {
	case WAVE_SINE:      return "Sine"sa;
	case WAVE_SAWTOOTH:  return "Sawtooth"sa;
	case WAVE_SQUARE:    return "Square"sa;
	case WAVE_TRIANGLE:  return "Triangle"sa;
	case WAVE_NOISE:     return "Noise"sa;
	default:             return ""sa;
	}
}

struct NodeWave {
	NodeHeader header;
	static const U32 TIME_INPUT_IDX = 0;
	static const U32 FREQUENCY_INPUT_IDX = 1;
	Waveform waveform = WAVE_SINE;

	void set_waveform(Waveform newWaveform) {
		waveform = newWaveform;
		if (UI::Box* box = header.uiNodeTitleBox.get()) {
			box->text = waveform_name(newWaveform);
		}
	}

	void init() {
		header.init(NODE_WAVE, "Basic Wave"sa);
		header.add_widget()->output.init();
		header.add_widget()->input.init(0.0);
		header.add_widget()->input.init(0.0);
		using namespace UI;
		BoxHandle dropdownBox = alloc_box();
		dropdownBox.unsafeBox->flags |= BOX_FLAG_INVISIBLE;
		dropdownBox.unsafeBox->layoutDirection = LAYOUT_DIRECTION_RIGHT;
		dropdownBox.unsafeBox->sizeParentPercent.x = 1.0F;
		UI_WORKING_BOX(dropdownBox) {
			spacer();
			BoxHandle waveformSelector = text_button("Waveform"sa, nullptr);
			waveformSelector.unsafeBox->userData[1] = UPtr(this);
			waveformSelector.unsafeBox->actionCallback = [](Box* box, UserCommunication& comm) {
				if (comm.leftClicked) {
					UI_BACKGROUND_COLOR((V4F32{ 0.15F, 0.15F, 0.15F, 1.0F }))
						UI_ADD_CONTEXT_MENU(BoxHandle{}, (V2F32{ comm.renderArea.minX, comm.renderArea.maxY })) {
						contextMenuBox.unsafeBox->contentScale = comm.scale;
						workingBox.unsafeBox->userData[1] = box->userData[1];
						BoxConsumer callback = [](Box* box) {
							reinterpret_cast<NodeWave*>(box->parent->userData[1])->set_waveform(Waveform(box->userData[1]));
						};

						for (Waveform op = WAVE_SINE; op <= WAVE_NOISE; op = Waveform(op + 1)) {
							text_button(waveform_name(op), callback).unsafeBox->userData[1] = UPtr(op);
						}
					}
				}
				return ACTION_PASS;
			};
			spacer();
		}
		header.add_widget()->customUIElement.init(dropdownBox);
	}

	void process() {
		NodeIOValue& time = header.get_input(TIME_INPUT_IDX)->value;
		NodeIOValue& frequency = header.get_input(FREQUENCY_INPUT_IDX)->value;
		NodeIOValue& output = header.get_output(0)->value;
		NodeIOValue* inputs[]{ &time, &frequency };
		NodeIOValue* outputs[]{ &output };
		switch (waveform) {
		case WAVE_SINE:
			for (U32 i = 0; i < output.bufferLength; i += 8) {
				__m256d d1 = _mm256_mul_pd(_mm256_load_pd(time.buffer + (i & time.bufferMask)), _mm256_load_pd(frequency.buffer + (i & frequency.bufferMask)));
				__m256d d2 = _mm256_mul_pd(_mm256_load_pd(time.buffer + ((i + 4) & time.bufferMask)), _mm256_load_pd(frequency.buffer + ((i + 4) & frequency.bufferMask)));
				// Truncate in double precision. Sine is fine in floating point as long as the value is in range
				d1 = _mm256_sub_pd(d1, _mm256_round_pd(d1, _MM_ROUND_MODE_DOWN));
				d2 = _mm256_sub_pd(d2, _mm256_round_pd(d2, _MM_ROUND_MODE_DOWN));
				__m256 sineResult = sinf32x8(_mm256_insertf128_ps(_mm256_castps128_ps256(_mm256_cvtpd_ps(d1)), _mm256_cvtpd_ps(d2), 1));
				_mm256_store_pd(output.buffer + i, _mm256_cvtps_pd(_mm256_castps256_ps128(sineResult)));
				_mm256_store_pd(output.buffer + i + 4, _mm256_cvtps_pd(_mm256_extractf128_ps(sineResult, 1)));
			}
			break;
		case WAVE_SAWTOOTH:
			for (U32 i = 0; i < output.bufferLength; i += 8) {
				__m256d d1 = _mm256_mul_pd(_mm256_load_pd(time.buffer + (i & time.bufferMask)), _mm256_load_pd(frequency.buffer + (i & frequency.bufferMask)));
				__m256d d2 = _mm256_mul_pd(_mm256_load_pd(time.buffer + ((i + 4) & time.bufferMask)), _mm256_load_pd(frequency.buffer + ((i + 4) & frequency.bufferMask)));
				d1 = _mm256_sub_pd(d1, _mm256_round_pd(d1, _MM_ROUND_MODE_DOWN));
				d2 = _mm256_sub_pd(d2, _mm256_round_pd(d2, _MM_ROUND_MODE_DOWN));
				__m256 sawtoothResult = _mm256_fmsub_ps(_mm256_set1_ps(2.0f), _mm256_insertf128_ps(_mm256_castps128_ps256(_mm256_cvtpd_ps(d1)), _mm256_cvtpd_ps(d2), 1), _mm256_set1_ps(1.0f));
				_mm256_store_pd(output.buffer + i, _mm256_cvtps_pd(_mm256_castps256_ps128(sawtoothResult)));
				_mm256_store_pd(output.buffer + i + 4, _mm256_cvtps_pd(_mm256_extractf128_ps(sawtoothResult, 1)));
			}
			break;
		case WAVE_SQUARE:
			for (U32 i = 0; i < output.bufferLength; i += 8) {
				__m256d d1 = _mm256_mul_pd(_mm256_load_pd(time.buffer + (i & time.bufferMask)), _mm256_load_pd(frequency.buffer + (i & frequency.bufferMask)));
				__m256d d2 = _mm256_mul_pd(_mm256_load_pd(time.buffer + ((i + 4) & time.bufferMask)), _mm256_load_pd(frequency.buffer + ((i + 4) & frequency.bufferMask)));
				d1 = _mm256_sub_pd(d1, _mm256_round_pd(d1, _MM_ROUND_MODE_DOWN));
				d2 = _mm256_sub_pd(d2, _mm256_round_pd(d2, _MM_ROUND_MODE_DOWN));
				__m256 squareResult = _mm256_blendv_ps(_mm256_set1_ps(-1.0f), _mm256_set1_ps(1.0f), _mm256_cmp_ps(_mm256_insertf128_ps(_mm256_castps128_ps256(_mm256_cvtpd_ps(d1)), _mm256_cvtpd_ps(d2), 1), _mm256_set1_ps(0.5f), _CMP_GE_OQ));
				_mm256_store_pd(output.buffer + i, _mm256_cvtps_pd(_mm256_castps256_ps128(squareResult)));
				_mm256_store_pd(output.buffer + i + 4, _mm256_cvtps_pd(_mm256_extractf128_ps(squareResult, 1)));
			}
			break;
		case WAVE_TRIANGLE:
			__m256 sign_mask = _mm256_castsi256_ps(_mm256_set1_epi32(0x7fffffff));
			for (U32 i = 0; i < output.bufferLength; i += 8) {
				__m256d d1 = _mm256_mul_pd(_mm256_load_pd(time.buffer + (i & time.bufferMask)), _mm256_load_pd(frequency.buffer + (i & frequency.bufferMask)));
				__m256d d2 = _mm256_mul_pd(_mm256_load_pd(time.buffer + ((i + 4) & time.bufferMask)), _mm256_load_pd(frequency.buffer + ((i + 4) & frequency.bufferMask)));
				d1 = _mm256_sub_pd(d1, _mm256_round_pd(d1, _MM_ROUND_MODE_DOWN));
				d2 = _mm256_sub_pd(d2, _mm256_round_pd(d2, _MM_ROUND_MODE_DOWN));
				__m256 absResult = _mm256_and_ps(_mm256_fmsub_ps(_mm256_set1_ps(2.0f), _mm256_insertf128_ps(_mm256_castps128_ps256(_mm256_cvtpd_ps(d1)), _mm256_cvtpd_ps(d2), 1), _mm256_set1_ps(1.0f)), sign_mask);
				__m256 triangleResult = _mm256_mul_ps(absResult, _mm256_set1_ps(2.0f));
				triangleResult = _mm256_sub_ps(triangleResult, _mm256_set1_ps(1.0f));
				_mm256_store_pd(output.buffer + i, _mm256_cvtps_pd(_mm256_castps256_ps128(triangleResult)));
				_mm256_store_pd(output.buffer + i + 4, _mm256_cvtps_pd(_mm256_extractf128_ps(triangleResult, 1)));
			}
			break;
		case WAVE_NOISE:
			for (U32 i = 0; i < output.bufferLength; i += 8) {
				__m256 noiseResult = random_f32();
				_mm256_store_pd(output.buffer + i, _mm256_cvtps_pd(_mm256_castps256_ps128(noiseResult)));
				_mm256_store_pd(output.buffer + i + 4, _mm256_cvtps_pd(_mm256_extractf128_ps(noiseResult, 1)));
			}
			break;
		}
	}
	void add_to_ui() {
		using namespace UI;
		Box* box = header.add_to_ui();
	}
};

enum FilterType {
	FILTER_LOWPASS,
	FILTER_HIGHPASS,
	FILTER_BANDPASS
};

const StrA filterTypeName(FilterType type) {
	switch (type) {
	case FILTER_LOWPASS: return "Lowpass"sa;
	case FILTER_HIGHPASS: return "Highpass"sa;
	case FILTER_BANDPASS: return "Bandpass"sa;
	default: return "Unknown"sa;
	}
}

struct NodeFilter {
	NodeHeader header;
	float cutoffFrequency;
	float resonance;
	U32 sampleRate;
	FilterType filterType;

	// Filter coefficients
	float a0, a1, a2, b0, b1, b2;
	float x_1, x_2, y_1, y_2;

	void init() {
		using namespace UI;
		header.init(NODE_FILTER, "Filter"sa);
		header.add_widget()->input.init(0.0f);
		header.add_widget()->input.init(1000.0f);
		header.add_widget()->input.init(0.7f);
		header.add_widget()->output.init();
		sampleRate = 44100;
		cutoffFrequency = 1000.0f;
		resonance = 0.7f;
		filterType = FILTER_LOWPASS;
		using namespace UI;
		BoxHandle dropdownBox = alloc_box();
		dropdownBox.unsafeBox->flags |= BOX_FLAG_INVISIBLE;
		dropdownBox.unsafeBox->layoutDirection = LAYOUT_DIRECTION_RIGHT;
		dropdownBox.unsafeBox->sizeParentPercent.x = 1.0F;

		UI_WORKING_BOX(dropdownBox) {
			spacer();
			BoxHandle filterSelector = text_button("Filter Type"sa, nullptr);
			filterSelector.unsafeBox->userData[1] = UPtr(this);
			filterSelector.unsafeBox->actionCallback = [](Box* box, UserCommunication& comm) {
				if (comm.leftClicked) {
					UI_BACKGROUND_COLOR((V4F32{ 0.15F, 0.15F, 0.15F, 1.0F }))
						UI_ADD_CONTEXT_MENU(BoxHandle{}, (V2F32{ comm.renderArea.minX, comm.renderArea.maxY })) {
						contextMenuBox.unsafeBox->contentScale = comm.scale;
						workingBox.unsafeBox->userData[1] = box->userData[1];
						BoxConsumer callback = [](Box* box) {
							NodeFilter* filter = reinterpret_cast<NodeFilter*>(box->parent->userData[1]);
							filter->filterType = static_cast<FilterType>(box->userData[1]);
							filter->setFilter(filter->cutoffFrequency, filter->resonance, filter->filterType);
						};

						for (int op = FILTER_LOWPASS; op <= FILTER_BANDPASS; ++op) {
							text_button(filterTypeName(static_cast<FilterType>(op)), callback).unsafeBox->userData[1] = UPtr(op);
						}
					}
				}
				return ACTION_PASS;
			};
			spacer();
		}
		header.add_widget()->customUIElement.init(dropdownBox);
	}

	void setFilterType(FilterType type) {
		filterType = type;
		if (UI::Box* box = header.uiNodeTitleBox.get()) {
			box->text = filterTypeName(type);
		}
	}

	void setFilter(float cutoff, float Q, FilterType type) {
		switch (type) {
		case FILTER_LOWPASS:
			setLowPass(cutoff, Q);
			break;
		case FILTER_HIGHPASS:
			setHighPass(cutoff, Q);
			break;
		case FILTER_BANDPASS:
			setBandPass(cutoff, Q);
			break;
		}
	}

	void setLowPass(float cutoff, float Q) {
		float w0 = 2.0f * 3.14159265 * cutoff / sampleRate;
		float alpha = sin(w0) / (2.0f * Q);
		float cosw0 = cos(w0);

		b0 = (1.0f - cosw0) / 2.0f;
		b1 = 1.0f - cosw0;
		b2 = b0;
		a0 = 1.0f + alpha;
		a1 = -2.0f * cosw0;
		a2 = 1.0f - alpha;

		b0 /= a0;
		b1 /= a0;
		b2 /= a0;
		a1 /= a0;
		a2 /= a0;
	}

	void setHighPass(float cutoff, float Q) {
		float w0 = 2.0f * 3.14159265 * cutoff / sampleRate;
		float alpha = sin(w0) / (2.0f * Q);
		float cosw0 = cos(w0);

		b0 = (1.0f + cosw0) / 2.0f;
		b1 = -(1.0f + cosw0);
		b2 = (1.0f + cosw0) / 2.0f;
		a0 = 1.0f + alpha;
		a1 = -2.0f * cosw0;
		a2 = 1.0f - alpha;

		b0 /= a0;
		b1 /= a0;
		b2 /= a0;
		a1 /= a0;
		a2 /= a0;
	}

	void setBandPass(float cutoff, float Q) {
		float w0 = 2.0f * 3.14159265 * cutoff / sampleRate;
		float alpha = sin(w0) / (2.0f * Q);
		float cosw0 = cos(w0);

		b0 = Q * alpha;
		b1 = 0.0f;
		b2 = -Q * alpha;
		a0 = 1.0f + alpha;
		a1 = -2.0f * cosw0;
		a2 = 1.0f - alpha;

		b0 /= a0;
		b1 /= a0;
		b2 /= a0;
		a1 /= a0;
		a2 /= a0;
	}

	float processSample(float in) {
		float out = b0 * in + b1 * x_1 + b2 * x_2 - a1 * y_1 - a2 * y_2;

		x_2 = x_1;
		x_1 = in;

		y_2 = y_1;
		y_1 = out;

		return out;
	}
	void process() {
		NodeIOValue& inputSignal = header.get_input(0)->value;
		NodeIOValue& cutoffControl = header.get_input(1)->value;
		NodeIOValue& resonanceControl = header.get_input(2)->value;
		NodeIOValue& output = header.get_output(0)->value;

		float newCutoff = std::max(20.0f, static_cast<float>(cutoffControl.buffer[0]));
		float newResonance = std::max(0.1f, static_cast<float>(resonanceControl.buffer[0]));
		if (cutoffFrequency != newCutoff || resonance != newResonance) {
			cutoffFrequency = newCutoff;
			resonance = newResonance;
			setFilter(cutoffFrequency, resonance, filterType);
		}

		for (U32 i = 0; i < output.bufferLength; i++) {
			output.buffer[i] = processSample(inputSignal.buffer[i]);
		}
	}

	void add_to_ui() {
		using namespace UI;
		Box* box = header.add_to_ui();
	}
};

enum MathOp {
	// Unary
	MATH_OP_NEG,
	MATH_OP_NOT,
	MATH_OP_ABS,
	MATH_OP_RCP,
	MATH_OP_SQRT,
	MATH_OP_RSQRT,
	// Binary
	MATH_OP_ADD,
	MATH_OP_SUB,
	MATH_OP_MUL,
	MATH_OP_DIV,
	MATH_OP_REM,
	// Binary comparison
	MATH_OP_EQ,
	MATH_OP_NE,
	MATH_OP_GT,
	MATH_OP_GE,
	MATH_OP_LT,
	MATH_OP_LE,
	// Binary logic
	MATH_OP_AND,
	MATH_OP_OR,
	MATH_OP_XOR
};
StrA math_op_name(MathOp m) {
	switch (m) {
	case MATH_OP_NEG:   return "Negate"sa;
	case MATH_OP_NOT:   return "Not"sa;
	case MATH_OP_ABS:   return "Abs"sa;
	case MATH_OP_RCP:   return "Rcp"sa;
	case MATH_OP_SQRT:  return "Sqrt"sa;
	case MATH_OP_RSQRT: return "Rcp Sqrt"sa;
	case MATH_OP_ADD:   return "Add"sa;
	case MATH_OP_SUB:   return "Subtract"sa;
	case MATH_OP_MUL:   return "Multiply"sa;
	case MATH_OP_DIV:   return "Divide"sa;
	case MATH_OP_REM:   return "Remainder"sa;
	case MATH_OP_EQ:    return "Equal"sa;
	case MATH_OP_NE:    return "Not Equal"sa;
	case MATH_OP_GT:    return "Greater"sa;
	case MATH_OP_GE:    return "Greater/Equal"sa;
	case MATH_OP_LT:    return "Less"sa;
	case MATH_OP_LE:    return "Less/Equal"sa;
	case MATH_OP_AND:   return "And"sa;
	case MATH_OP_OR:    return "Or"sa;
	case MATH_OP_XOR:   return "Xor"sa;
	}
	return ""sa;
}
struct NodeMathOp {
	NodeHeader header;
	MathOp op;

	void set_op(MathOp newOp) {
		op = newOp;
		if (UI::Box* box = header.uiNodeTitleBox.get()) {
			box->text = math_op_name(newOp);
		}
		//TODO disable second input if it's a unary operation
	}

	void init() {
		header.init(NODE_MATH, "Math"sa);
		{
			using namespace UI;
			BoxHandle dropdownBox = alloc_box();
			dropdownBox.unsafeBox->flags |= BOX_FLAG_INVISIBLE;
			dropdownBox.unsafeBox->layoutDirection = LAYOUT_DIRECTION_RIGHT;
			dropdownBox.unsafeBox->sizeParentPercent.x = 1.0F;
			UI_WORKING_BOX(dropdownBox) {
				spacer();
				BoxHandle operationSelector = text_button("Operation"sa, nullptr);
				operationSelector.unsafeBox->userData[1] = UPtr(this);
				operationSelector.unsafeBox->actionCallback = [](Box* box, UserCommunication& comm) {
					if (comm.leftClicked) {
						UI_BACKGROUND_COLOR((V4F32{ 0.15F, 0.15F, 0.15F, 1.0F }))
						UI_ADD_CONTEXT_MENU(BoxHandle{}, (V2F32{ comm.renderArea.minX, comm.renderArea.maxY })) {
							contextMenuBox.unsafeBox->contentScale = comm.scale;
							workingBox.unsafeBox->userData[1] = box->userData[1];
							BoxConsumer callback = [](Box* box) {
								reinterpret_cast<NodeMathOp*>(box->parent->userData[1])->set_op(MathOp(box->userData[1]));
							};

							for (MathOp op = MATH_OP_NEG; op <= MATH_OP_RSQRT; op = MathOp(U32(op) + 1)) {
								text_button(math_op_name(op), callback).unsafeBox->userData[1] = op;
							}

							UI_SIZE((V2F32{ 64.0F, 2.0F }))
							UI_BACKGROUND_COLOR((V4F32{ 0.1F, 0.1F, 0.1F, 1.0F }))
							generic_box();

							for (MathOp op = MATH_OP_ADD; op <= MATH_OP_REM; op = MathOp(U32(op) + 1)) {
								text_button(math_op_name(op), callback).unsafeBox->userData[1] = op;
							}

							UI_SIZE((V2F32{ 64.0F, 2.0F }))
							UI_BACKGROUND_COLOR((V4F32{ 0.1F, 0.1F, 0.1F, 1.0F }))
							generic_box();

							for (MathOp op = MATH_OP_EQ; op <= MATH_OP_LE; op = MathOp(U32(op) + 1)) {
								text_button(math_op_name(op), callback).unsafeBox->userData[1] = op;
							}

							UI_SIZE((V2F32{ 64.0F, 2.0F }))
							UI_BACKGROUND_COLOR((V4F32{ 0.1F, 0.1F, 0.1F, 1.0F }))
							generic_box();

							for (MathOp op = MATH_OP_AND; op <= MATH_OP_XOR; op = MathOp(U32(op) + 1)) {
								text_button(math_op_name(op), callback).unsafeBox->userData[1] = op;
							}
						}
					}
					return ACTION_PASS;
				};
				spacer();
			}
			header.add_widget()->customUIElement.init(dropdownBox);
		}
		header.add_widget()->output.init();
		header.add_widget()->input.init(0.0);
		header.add_widget()->input.init(0.0);
		op = MATH_OP_ADD;
	}
	void process() {
		NodeIOValue& operandA = header.get_input(0)->value;
		NodeIOValue& operandB = header.get_input(1)->value;
		NodeIOValue& output = header.get_output(0)->value;
		switch (op) {
		case MATH_OP_NEG: {
			__m256d signBit = _mm256_castsi256_pd(_mm256_set1_epi64x(0x8000000000000000ull));
			for (U32 i = 0; i < output.bufferLength; i += 4) {
				_mm256_store_pd(output.buffer + i, _mm256_xor_pd(_mm256_load_pd(operandA.buffer + (i & operandA.bufferMask)), signBit));
			}
		} break;
		case MATH_OP_NOT: {
			__m256d one = _mm256_set1_pd(1.0F);
			for (U32 i = 0; i < output.bufferLength; i += 4) {
				__m256d isNotZero = _mm256_cmp_pd(_mm256_load_pd(operandA.buffer + (i & operandA.bufferMask)), _mm256_setzero_pd(), _CMP_NEQ_OQ);
				_mm256_store_pd(output.buffer + i, _mm256_and_pd(one, isNotZero));
			}
		} break;
		case MATH_OP_ABS: {
			__m256d maskOffSignBit = _mm256_castsi256_pd(_mm256_set1_epi64x(0x7FFFFFFFFFFFFFFFull));
			for (U32 i = 0; i < output.bufferLength; i += 4) {
				_mm256_store_pd(output.buffer + i, _mm256_and_pd(_mm256_load_pd(operandA.buffer + (i & operandA.bufferMask)), maskOffSignBit));
			}
		} break;
		case MATH_OP_RCP: {
			for (U32 i = 0; i < output.bufferLength; i += 8) {
				__m256d d1 = _mm256_load_pd(operandA.buffer + (i & operandA.bufferMask));
				__m256d d2 = _mm256_load_pd(operandA.buffer + ((i + 4) & operandA.bufferMask));
				__m256 rcpResult = _mm256_rcp_ps(_mm256_insertf128_ps(_mm256_castps128_ps256(_mm256_cvtpd_ps(d1)), _mm256_cvtpd_ps(d2), 1));
				_mm256_store_pd(output.buffer + i, _mm256_cvtps_pd(_mm256_castps256_ps128(rcpResult)));
				_mm256_store_pd(output.buffer + i + 4, _mm256_cvtps_pd(_mm256_extractf128_ps(rcpResult, 1)));
			}
		} break;
		case MATH_OP_SQRT: {
			for (U32 i = 0; i < output.bufferLength; i += 4) {
				_mm256_store_pd(output.buffer + i, _mm256_sqrt_pd(_mm256_load_pd(operandA.buffer + (i & operandA.bufferMask))));
			}
		} break;
		case MATH_OP_RSQRT: {
			for (U32 i = 0; i < output.bufferLength; i += 8) {
				__m256d d1 = _mm256_load_pd(operandA.buffer + (i & operandA.bufferMask));
				__m256d d2 = _mm256_load_pd(operandA.buffer + ((i + 4) & operandA.bufferMask));
				__m256 rsqrtResult = _mm256_rsqrt_ps(_mm256_insertf128_ps(_mm256_castps128_ps256(_mm256_cvtpd_ps(d1)), _mm256_cvtpd_ps(d2), 1));
				_mm256_store_pd(output.buffer + i, _mm256_cvtps_pd(_mm256_castps256_ps128(rsqrtResult)));
				_mm256_store_pd(output.buffer + i + 4, _mm256_cvtps_pd(_mm256_extractf128_ps(rsqrtResult, 1)));
			}
		} break;
		case MATH_OP_ADD: {
			for (U32 i = 0; i < output.bufferLength; i += 4) {
				_mm256_store_pd(output.buffer + i, _mm256_add_pd(_mm256_load_pd(operandA.buffer + (i & operandA.bufferMask)), _mm256_load_pd(operandB.buffer + (i & operandB.bufferMask))));
			}
		} break;
		case MATH_OP_SUB: {
			for (U32 i = 0; i < output.bufferLength; i += 4) {
				_mm256_store_pd(output.buffer + i, _mm256_sub_pd(_mm256_load_pd(operandA.buffer + (i & operandA.bufferMask)), _mm256_load_pd(operandB.buffer + (i & operandB.bufferMask))));
			}
		} break;
		case MATH_OP_MUL: {
			for (U32 i = 0; i < output.bufferLength; i += 4) {
				_mm256_store_pd(output.buffer + i, _mm256_mul_pd(_mm256_load_pd(operandA.buffer + (i & operandA.bufferMask)), _mm256_load_pd(operandB.buffer + (i & operandB.bufferMask))));
			}
		} break;
		case MATH_OP_DIV: {
			for (U32 i = 0; i < output.bufferLength; i += 4) {
				_mm256_store_pd(output.buffer + i, _mm256_div_pd(_mm256_load_pd(operandA.buffer + (i & operandA.bufferMask)), _mm256_load_pd(operandB.buffer + (i & operandB.bufferMask))));
			}
		} break;
		case MATH_OP_REM: {
			for (U32 i = 0; i < output.bufferLength; i += 4) {
				__m256d x = _mm256_load_pd(operandA.buffer + (i & operandA.bufferMask));
				__m256d y = _mm256_load_pd(operandB.buffer + (i & operandB.bufferMask));
				__m256d isZero = _mm256_cmp_pd(y, _mm256_setzero_pd(), _CMP_EQ_UQ);
				__m256d val = _mm256_sub_pd(x, _mm256_mul_pd(_mm256_round_pd(_mm256_div_pd(x, y), _MM_FROUND_TO_ZERO | _MM_FROUND_NO_EXC), y));
				_mm256_store_pd(output.buffer + i, _mm256_blendv_pd(val, _mm256_setzero_pd(), isZero));
			}
		} break;
		case MATH_OP_EQ: {
			__m256d one = _mm256_set1_pd(1.0F);
			for (U32 i = 0; i < output.bufferLength; i += 4) {
				__m256d isEQ = _mm256_cmp_pd(_mm256_load_pd(operandA.buffer + (i & operandA.bufferMask)), _mm256_load_pd(operandB.buffer + (i & operandB.bufferMask)), _CMP_EQ_OQ);
				_mm256_store_pd(output.buffer + i, _mm256_and_pd(one, isEQ));
			}
		} break;
		case MATH_OP_NE: {
			__m256d one = _mm256_set1_pd(1.0F);
			for (U32 i = 0; i < output.bufferLength; i += 4) {
				__m256d isNE = _mm256_cmp_pd(_mm256_load_pd(operandA.buffer + (i & operandA.bufferMask)), _mm256_load_pd(operandB.buffer + (i & operandB.bufferMask)), _CMP_NEQ_OQ);
				_mm256_store_pd(output.buffer + i, _mm256_and_pd(one, isNE));
			}
		} break;
		case MATH_OP_GT: {
			__m256d one = _mm256_set1_pd(1.0F);
			for (U32 i = 0; i < output.bufferLength; i += 4) {
				__m256d isGT = _mm256_cmp_pd(_mm256_load_pd(operandA.buffer + (i & operandA.bufferMask)), _mm256_load_pd(operandB.buffer + (i & operandB.bufferMask)), _CMP_GT_OQ);
				_mm256_store_pd(output.buffer + i, _mm256_and_pd(one, isGT));
			}
		} break;
		case MATH_OP_GE: {
			__m256d one = _mm256_set1_pd(1.0F);
			for (U32 i = 0; i < output.bufferLength; i += 4) {
				__m256d isGE = _mm256_cmp_pd(_mm256_load_pd(operandA.buffer + (i & operandA.bufferMask)), _mm256_load_pd(operandB.buffer + (i & operandB.bufferMask)), _CMP_GE_OQ);
				_mm256_store_pd(output.buffer + i, _mm256_and_pd(one, isGE));
			}
		} break;
		case MATH_OP_LT: {
			__m256d one = _mm256_set1_pd(1.0F);
			for (U32 i = 0; i < output.bufferLength; i += 4) {
				__m256d isLT = _mm256_cmp_pd(_mm256_load_pd(operandA.buffer + (i & operandA.bufferMask)), _mm256_load_pd(operandB.buffer + (i & operandB.bufferMask)), _CMP_LT_OQ);
				_mm256_store_pd(output.buffer + i, _mm256_and_pd(one, isLT));
			}
		} break;
		case MATH_OP_LE: {
			__m256d one = _mm256_set1_pd(1.0F);
			for (U32 i = 0; i < output.bufferLength; i += 4) {
				__m256d isLE = _mm256_cmp_pd(_mm256_load_pd(operandA.buffer + (i & operandA.bufferMask)), _mm256_load_pd(operandB.buffer + (i & operandB.bufferMask)), _CMP_LE_OQ);
				_mm256_store_pd(output.buffer + i, _mm256_and_pd(one, isLE));
			}
		} break;
		case MATH_OP_AND: {
			__m256d one = _mm256_set1_pd(1.0F);
			for (U32 i = 0; i < output.bufferLength; i += 4) {
				__m256d aIsNotZero = _mm256_cmp_pd(_mm256_load_pd(operandA.buffer + (i & operandA.bufferMask)), _mm256_setzero_pd(), _CMP_NEQ_OQ);
				__m256d bIsNotZero = _mm256_cmp_pd(_mm256_load_pd(operandB.buffer + (i & operandB.bufferMask)), _mm256_setzero_pd(), _CMP_NEQ_OQ);
				_mm256_store_pd(output.buffer + i, _mm256_and_pd(one, _mm256_and_pd(aIsNotZero, bIsNotZero)));
			}
		} break;
		case MATH_OP_OR: {
			__m256d one = _mm256_set1_pd(1.0F);
			for (U32 i = 0; i < output.bufferLength; i += 4) {
				__m256d aIsNotZero = _mm256_cmp_pd(_mm256_load_pd(operandA.buffer + (i & operandA.bufferMask)), _mm256_setzero_pd(), _CMP_NEQ_OQ);
				__m256d bIsNotZero = _mm256_cmp_pd(_mm256_load_pd(operandB.buffer + (i & operandB.bufferMask)), _mm256_setzero_pd(), _CMP_NEQ_OQ);
				_mm256_store_pd(output.buffer + i, _mm256_and_pd(one, _mm256_or_pd(aIsNotZero, bIsNotZero)));
			}
		} break;
		case MATH_OP_XOR: {
			__m256d one = _mm256_set1_pd(1.0F);
			for (U32 i = 0; i < output.bufferLength; i += 4) {
				__m256d aIsNotZero = _mm256_cmp_pd(_mm256_load_pd(operandA.buffer + (i & operandA.bufferMask)), _mm256_setzero_pd(), _CMP_NEQ_OQ);
				__m256d bIsNotZero = _mm256_cmp_pd(_mm256_load_pd(operandB.buffer + (i & operandB.bufferMask)), _mm256_setzero_pd(), _CMP_NEQ_OQ);
				_mm256_store_pd(output.buffer + i, _mm256_and_pd(one, _mm256_xor_pd(aIsNotZero, bIsNotZero)));
			}
		} break;
		}
	}
	void add_to_ui() {
		using namespace UI;
		Box* box = header.add_to_ui();
	}
};
struct NodeOscilloscope {
	NodeHeader header;
	static const U32 TIME_INPUT_IDX = 0;

	void init() {
		header.init(NODE_OSCILLOSCOPE, "Oscilloscope"sa);
		header.add_widget()->oscilloscope.init();
		header.add_widget()->input.init(0.0);
	}
	void process() {
		NodeIOValue& input = header.get_input(TIME_INPUT_IDX)->value;
		double* audioDataDouble = input.buffer;
		U32 bufferSize = input.bufferLength;
		NodeWidgetOscilloscope* oscWidget = reinterpret_cast<NodeWidgetOscilloscope*>(header.get_nth_of_type(NODE_WIDGET_OSCILLOSCOPE, 0));
		if (oscWidget) {
			oscWidget->updateWaveform(input.buffer, bufferSize);
		}
		else {
			print("Oscilloscope widget not found or not initialized.\n");
		}
	}
	void add_to_ui() {
		using namespace UI;
		Box* box = header.add_to_ui();
	}
};
struct NodeSampler {
	NodeHeader header;
	static const U32 TIME_INPUT_IDX = 0;

	void init() {
		header.init(NODE_SAMPLER, "Sampler"sa);
		header.add_widget()->output.init();
		header.add_widget()->input.init(0.0);
		header.add_widget()->file_dialog_button.init();
	}
	void process() {
		NodeWidgetSamplerButton& button = *header.get_samplerbutton(0);
		if (!button.audioData) return;
		NodeIOValue& time = header.get_input(TIME_INPUT_IDX)->value;
		NodeIOValue& output = header.get_output(0)->value;

		__m256d sampleCount = _mm256_set1_pd(F64(button.numSamples));
		__m256i sampleCountMinus1 = _mm256_set1_epi32(button.numSamples - 1);
		__m256i sampleCountMinus2 = _mm256_set1_epi32(button.numSamples - 2);
		__m256d rcpSampleLengthSeconds = _mm256_set1_pd(F64(button.sampleRate) / F64(button.numSamples));
		__m256d sampleRate = _mm256_set1_pd(F64(button.sampleRate));
		__m256i oneI32 = _mm256_set1_epi32(1);
		__m256i twoI32 = _mm256_set1_epi32(2);
		__m256 twoF32 = _mm256_set1_ps(2.0F);
		__m256 threeF32 = _mm256_set1_ps(3.0F);
		U32 oldRoundingMode = _MM_GET_ROUNDING_MODE();
		_MM_SET_ROUNDING_MODE(_MM_ROUND_TOWARD_ZERO);
		for (U32 i = 0; i < output.bufferLength; i += 8) {
			__m256d timeInput0 = _mm256_load_pd(time.buffer + (i & time.bufferMask));
			__m256d timeInput1 = _mm256_load_pd(time.buffer + (i + 4 & time.bufferMask));
			__m256d normalizedTime0 = _mm256_mul_pd(rcpSampleLengthSeconds, timeInput0);
			__m256d normalizedTime1 = _mm256_mul_pd(rcpSampleLengthSeconds, timeInput1);
			normalizedTime0 = _mm256_sub_pd(normalizedTime0, _mm256_round_pd(normalizedTime0, _MM_FROUND_TO_NEG_INF | _MM_FROUND_NO_EXC));
			normalizedTime1 = _mm256_sub_pd(normalizedTime1, _mm256_round_pd(normalizedTime1, _MM_FROUND_TO_NEG_INF | _MM_FROUND_NO_EXC));
			
			__m256i indices = _mm256_inserti128_si256(_mm256_castsi128_si256(_mm256_cvtpd_epi32(_mm256_mul_pd(normalizedTime0, sampleCount))), _mm256_cvtpd_epi32(_mm256_mul_pd(normalizedTime1, sampleCount)), 1);
			__m256 x0 = _mm256_mask_i32gather_ps(_mm256_setzero_ps(), button.audioData, _mm256_sub_epi32(indices, oneI32), _mm256_castsi256_ps(_mm256_cmpgt_epi32(indices, _mm256_setzero_si256())), 4);
			__m256 x1 = _mm256_i32gather_ps(button.audioData, indices, 4);
			__m256 x2 = _mm256_mask_i32gather_ps(_mm256_setzero_ps(), button.audioData, _mm256_add_epi32(indices, oneI32), _mm256_castsi256_ps(_mm256_cmpgt_epi32(sampleCountMinus1, indices)), 4);
			__m256 x3 = _mm256_mask_i32gather_ps(_mm256_setzero_ps(), button.audioData, _mm256_add_epi32(indices, twoI32), _mm256_castsi256_ps(_mm256_cmpgt_epi32(sampleCountMinus2, indices)), 4);
			__m256 slope0 = _mm256_sub_ps(x1, x0);
			__m256 slope1 = _mm256_sub_ps(x3, x2);

			// (2.0 * x1 + slope0) - (2.0 * x2 - slope1)
			__m256 a = _mm256_sub_ps(_mm256_fmadd_ps(twoF32, x1, slope0), _mm256_fmsub_ps(twoF32, x2, slope1));
			// -3.0 * x1 - (2.0 * slope0 - (3.0 * x2 - slope1));
			__m256 b = _mm256_fnmsub_ps(threeF32, x1, _mm256_fmsub_ps(twoF32, slope0, _mm256_fmsub_ps(threeF32, x2, slope1)));
			__m256 c = slope0;
			__m256 d = x1;
			__m256d t0F64 = _mm256_mul_pd(sampleRate, timeInput0);
			__m256d t1F64 = _mm256_mul_pd(sampleRate, timeInput1);
			t0F64 = _mm256_sub_pd(t0F64, _mm256_round_pd(t0F64, _MM_FROUND_TO_NEG_INF | _MM_FROUND_NO_EXC));
			t1F64 = _mm256_sub_pd(t1F64, _mm256_round_pd(t1F64, _MM_FROUND_TO_NEG_INF | _MM_FROUND_NO_EXC));
			__m256 t = _mm256_insertf128_ps(_mm256_castps128_ps256(_mm256_cvtpd_ps(t0F64)), _mm256_cvtpd_ps(t1F64), 1);
			__m256 val = _mm256_fmadd_ps(_mm256_fmadd_ps(_mm256_fmadd_ps(a, t, b), t, c), t, d);

			_mm256_store_pd(output.buffer + i + 0, _mm256_cvtps_pd(_mm256_extractf128_ps(val, 0)));
			_mm256_store_pd(output.buffer + i + 4, _mm256_cvtps_pd(_mm256_extractf128_ps(val, 1)));
		}
		_MM_SET_ROUNDING_MODE(oldRoundingMode);
	}
	void add_to_ui() {
		using namespace UI;
		Box* box = header.add_to_ui();
	}
};
union Node {
	Node* freeListNextPtr;
	NodeHeader header;
#define X(enumName, typeName) typeName typeName##_val;
	NODES
#undef X
};

// This code is basically the same as alloc_widget. Perhaps I ought to have some sort of generic block allocator
Node* freeNodeListHead = nullptr;
Node* alloc_node() {
	if (!freeNodeListHead) {
		const U32 amountToAlloc = 256;
		Node* block = globalArena.alloc<Node>(amountToAlloc);
		for (U32 i = 0; i < amountToAlloc; i++) {
			block[i].freeListNextPtr = &block[i + 1];
		}
		block[amountToAlloc - 1].freeListNextPtr = nullptr;
		freeNodeListHead = block;
	}
	Node* allocated = freeNodeListHead;
	freeNodeListHead = allocated->freeListNextPtr;
	return allocated;
}
void free_node(Node* toFree) {
	toFree->freeListNextPtr = freeNodeListHead;
	freeNodeListHead = toFree;
}

void process_node(NodeHeader* node) {
	if (node->hasProcessed) {
		return;
	}
	node->hasProcessed = true;
	ArenaArrayList<NodeIOValue*> inputs{ &audioArena };
	ArenaArrayList<NodeIOValue*> outputs{ &audioArena };
	for (NodeWidgetHeader* widget = node->widgetBegin; widget != nullptr; widget = widget->next) {
		if (widget->type == NODE_WIDGET_INPUT) {
			NodeWidgetInput* inWidget = reinterpret_cast<NodeWidgetInput*>(widget);
			if (NodeWidgetOutput* input = inWidget->inputHandle.get()) {
				process_node(input->header.parent);
				inWidget->value = input->value;
				if (inWidget->program.valid && (!inWidget->inputHandle.get() || inWidget->program.operatesOnBuffer)) {
					if (inWidget->value.bufferMask == U32_MAX) {
						for (U32 i = 0; i < inWidget->value.bufferLength; i += 4) {
							_mm256_store_pd(inWidget->value.buffer + i, interpret(inWidget->program, _mm256_load_pd(inWidget->value.buffer + i)));
						}
					}
					else {
						tbrs::AVX2D result = interpret(inWidget->program, _mm256_load_pd(inWidget->value.buffer));
						_mm256_store_pd(inWidget->value.buffer, result);
						_mm256_store_pd(inWidget->value.buffer + 4, result);
					}
				}
			}
			else {
				inWidget->value.set_scalar(inWidget->defaultValue);
				if (inWidget->program.valid) {
					tbrs::AVX2D result = interpret(inWidget->program, _mm256_load_pd(inWidget->value.buffer));
					_mm256_store_pd(inWidget->value.buffer, result);
					_mm256_store_pd(inWidget->value.buffer + 4, result);
				}
			}
			inputs.push_back(&inWidget->value);
		} else if (widget->type == NODE_WIDGET_OUTPUT) {
			outputs.push_back(&reinterpret_cast<NodeWidgetOutput*>(widget)->value);
		}
	}
	make_node_io_consistent(inputs.data, inputs.size, outputs.data, outputs.size);
#define X(enumName, typeName) case NODE_##enumName: reinterpret_cast<typeName*>(node)->process(); break;
	switch (node->type) {
	NODES
	default: break;
	}
#undef X
}

void add_node_to_ui(UI::BoxHandle parent, NodeHeader* node) {
	UI::BoxHandle oldWorkingBox = UI::workingBox;
	UI::workingBox = parent;
#define X(enumName, typeName) case NODE_##enumName: reinterpret_cast<typeName*>(node)->add_to_ui(); break;
	switch (node->type) {
	NODES
	default: break;
	}
#undef X
	UI::workingBox = oldWorkingBox;
}

void add_widget_to_ui(NodeWidgetHeader* widget) {
#define X(enumName, typeName) case NODE_WIDGET_##enumName: reinterpret_cast<typeName*>(widget)->add_to_ui(); break;
	switch (widget->type) {
		NODE_WIDGETS
	default: break;
	}
#undef X
}

struct NodeGraph {
	NodeHeader* nodesFirst;
	NodeHeader* nodesLast;

	NodeHeader* selectedFirst;
	NodeHeader* selectedLast;
	NodeHeader* active;

	NodeChannelOut* outputsFirst;
	NodeChannelOut* outputsLast;

	F64* currentTimeBuffer;

	UI::BoxHandle uiBox;

	template<typename NodeT, typename... Args>
	NodeT& create_node(V2F32 pos, Args... args) {
		NodeT* node = reinterpret_cast<NodeT*>(alloc_node());
		node->init(args...);
		node->header.parent = this;
		node->header.offset = pos;
		add_node_to_ui(uiBox, &node->header);
		DLL_INSERT_TAIL(&node->header, nodesFirst, nodesLast, prev, next);
		if (node->header.type == NODE_CHANNEL_OUT) {
			DLL_INSERT_TAIL(reinterpret_cast<NodeChannelOut*>(node), outputsFirst, outputsLast, outputPrev, outputNext);
		}
		return *node;
	}

	void select_all() {
		selectedFirst = nodesFirst;
		selectedLast = nodesLast;
		for (NodeHeader* node = nodesFirst; node; node = node->selectedNext) {
			node->selectedNext = node->next;
			node->selectedPrev = node->prev;
		}
		active = nullptr;
	}
	void select(NodeHeader* node) {
		if (!node->selectedNext) {
			DLL_INSERT_TAIL(node, selectedFirst, selectedLast, selectedPrev, selectedNext);
			active = node;
		}
	}
	void deselect(NodeHeader* node) {
		if (node->selectedNext) {
			DLL_REMOVE(node, selectedFirst, selectedLast, selectedPrev, selectedNext);
			if (node == active) {
				active = nullptr;
			}
		}
	}
	void delete_node(NodeHeader* node) {
		DLL_REMOVE(node, nodesFirst, nodesLast, prev, next);
		if (node->selectedNext) {
			DLL_REMOVE(node, selectedFirst, selectedLast, selectedPrev, selectedNext);
		}
		if (node->type == NODE_CHANNEL_OUT) {
			DLL_REMOVE(reinterpret_cast<NodeChannelOut*>(node), outputsFirst, outputsLast, outputPrev, outputNext);
		}
		node->destroy();
	}
	void delete_all_nodes() {
		while (nodesFirst != nullptr) {
			NodeHeader* nextNode = nodesFirst->next;
			delete_node(nodesFirst);
			nodesFirst = nextNode;
		}
	}
	void delete_selected() {
		for (NodeHeader* node = selectedFirst; node;) {
			NodeHeader* nextNode = node->selectedNext;
			delete_node(node);
			node = nextNode;
		}
		active = nullptr;
	}
	void init() {
		*this = NodeGraph{};
		uiBox = UI::alloc_box();
		uiBox.unsafeBox->flags = UI::BOX_FLAG_INVISIBLE;
	}
	void destroy() {
		for (NodeHeader* node = selectedFirst; node; node = node->selectedNext) {
			node->destroy();
		}
	}

	void generate_output(F32 outputBuf[PROCESS_BUFFER_SIZE], F64 time[PROCESS_BUFFER_SIZE]) {
		currentTimeBuffer = time;
		audioArena.reset();
		for (NodeHeader* node = nodesFirst; node; node = node->next) {
			node->hasProcessed = false;
		}
		memset(outputBuf, 0, PROCESS_BUFFER_SIZE * sizeof(F32));
		for (NodeHeader* node = nodesFirst; node; node = node->next) {
			node->hasProcessed = false;
			process_node(node);
		}
		for (NodeChannelOut* output = outputsFirst; output; output = output->outputNext) {
			process_node(&output->header);
			U32 bufSize;
			U32 bufMask;
			F64* buf = output->get_output_buffer(&bufSize, &bufMask);
			if (buf && bufSize == PROCESS_BUFFER_SIZE) {
				__m256d scale = _mm256_set1_pd(0.5);
				for (U32 i = 0; i < PROCESS_BUFFER_SIZE; i += 8) {
					__m128 d1 = _mm256_cvtpd_ps(_mm256_mul_pd(scale, _mm256_load_pd(buf + (i & bufMask))));
					__m128 d2 = _mm256_cvtpd_ps(_mm256_mul_pd(scale, _mm256_load_pd(buf + ((i + 4) & bufMask))));
					__m256 result = _mm256_load_ps(outputBuf + i);
					result = _mm256_add_ps(result, _mm256_insertf128_ps(_mm256_castps128_ps256(d1), d2, 1));
					_mm256_store_ps(outputBuf + i, result);
				}
			}
		}
	}

	void draw_connections(DynamicVertexBuffer::Tessellator& tes, F32 z, U32 clipBoxIndex) {
		for (NodeHeader* node = nodesFirst; node; node = node->next) {
			for (NodeWidgetHeader* widget = node->widgetBegin; widget; widget = widget->next) {
				if (widget->type == NODE_WIDGET_INPUT) {
					NodeWidgetInput& input = *reinterpret_cast<NodeWidgetInput*>(widget);
					if (NodeWidgetOutput* output = input.inputHandle.get()) {
						V2F32 handleOffset = V2F32{ distance(input.connectionRenderPos, output->connectionRenderPos) * 0.5F, 0.0F };
						tes.ui_bezier_curve(output->connectionRenderPos, output->connectionRenderPos + handleOffset, input.connectionRenderPos - handleOffset, input.connectionRenderPos, z - 0.05F, 32, 2.0F, V4F32{ 0.8F, 0.8F, 0.8F, 1.0F }, Textures::simpleWhite.index, clipBoxIndex << 16);
					}
				}
			}
		}
	}
};

UI::Box* NodeHeader::add_to_ui() {
	using namespace UI;
	uiBox = generic_box();
	Box* box = uiBox.unsafeBox;
	box->layoutDirection = LAYOUT_DIRECTION_DOWN;
	box->backgroundColor = V4F32{ 0.0F, 1.0F, 0.0F, 1.0F }.to_rgba8();
	box->minSize = V2F32{ 120.0F, 200.0F };
	box->flags = BOX_FLAG_FLOATING_X | BOX_FLAG_FLOATING_Y | BOX_FLAG_DONT_LAYOUT_TO_FIT_CHILDREN;
	box->contentOffset = offset;
	box->zOffset = -0.1F;
	box->userData[0] = UPtr(this);
	box->actionCallback = [](Box* box, UserCommunication& comm) {
		if (comm.drag.x != 0.0F || comm.drag.y != 0.0F) {
			box->contentOffset += comm.drag;
			reinterpret_cast<NodeHeader*>(box->userData[0])->offset = box->contentOffset;
			return ACTION_HANDLED;
		}
		if (comm.leftClickStart) {
			UI::move_box_to_front(box);
		}
		return ACTION_PASS;
	};
	workingBox = uiBox;

	UI_RBOX() {
		spacer();
		UI_TEXT_SIZE(12.0F)
		UI_TEXT_COLOR((V4F32{ 0.2F, 0.0F, 0.0F, 1.0F }))
		uiNodeTitleBox = str_a(StrA{ title, titleLength });
		spacer();
		UI_SIZE((V2F32{ 8.0F, 8.0F }));
		button(Textures::uiX, [](Box* box) {
			reinterpret_cast<NodeHeader*>(box->userData[1])->parent->delete_node(reinterpret_cast<NodeHeader*>(box->userData[1]));
		}).unsafeBox->userData[1] = UPtr(this);
	}
	for (NodeWidgetHeader* widget = widgetBegin; widget; widget = widget->next) {
		add_widget_to_ui(widget);
		spacer(2.0F);
	}
	
	return box;
}

void NodeTimeIn::process() {
	NodeWidgetOutput& output = *header.get_output(0);
	if (header.parent->currentTimeBuffer) {
		output.value = NodeIOValue{};
		output.value.buffer = header.parent->currentTimeBuffer;
		output.value.bufferLength = PROCESS_BUFFER_SIZE;
		output.value.bufferMask = U32_MAX;
	} else {
		output.value.set_scalar(0.0);
	}
}

}