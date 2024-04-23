#pragma once
#include <commdlg.h>
#include <libsoundwave/AudioDecoder.h>
#include <filesystem>
#include "DrillLib.h"
#include "UI.h"

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
	X(SAMPLER_BUTTON, NodeWidgetSamplerButton)

#define NODES \
	X(TIME_IN, NodeTimeIn)\
	X(CHANNEL_OUT, NodeChannelOut)\
	X(SINE, NodeSine)\
	X(MATH, NodeMathOp)\
	X(OSCILLOSCOPE, NodeOscilloscope)\
	X(SAMPLER, NodeSampler)

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

	V2F32 connectionRenderPos;

	void init(F64 defaultVal) {
		header.init(NODE_WIDGET_INPUT);
		defaultValue = defaultVal;
		inputHandle = NodeWidgetHandle<NodeWidgetOutput>{};
	}

	void connect(NodeWidgetOutput* outputWidget) {
		inputHandle = NodeWidgetHandle<NodeWidgetOutput>{ outputWidget, outputWidget->header.generation };
	}

	NodeIOValue& eval() {
		if (NodeWidgetOutput* input = inputHandle.get()) {
			process_node(input->header.parent);
			value = input->value;
		} else {
			value.set_scalar(defaultValue);
		}
		return value;
	}

	void add_to_ui() {
		using namespace UI;
		UI_RBOX() {
			workingBox.unsafeBox->backgroundColor = V4F32{ 0.05F, 0.05F, 0.05F, 1.0F }.to_rgba8();
			workingBox.unsafeBox->flags &= ~BOX_FLAG_INVISIBLE;
			spacer(6.0F);
			UI_BACKGROUND_COLOR((V4F32{ 0.1F, 0.1F, 0.1F, 0.0F }))
			text_input("Input Number"sa, ""sa, [](Box* box) {
				NodeWidgetInput& input = *reinterpret_cast<NodeWidgetInput*>(box->userData[1]);
				if (!input.inputHandle.get()) {
					StrA parseStr{ box->typedTextBuffer, box->numTypedCharacters };
					if (ParseTools::parse_f64(&input.defaultValue, &parseStr)) {
						input.value.set_scalar(input.defaultValue);
					}
				}
			}).unsafeBox->userData[1] = UPtr(this);
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
	std::vector<F32> waveformBuffer;
	U32 sampleRate;

	void init() {
		header.init(NODE_WIDGET_OSCILLOSCOPE);
		waveformBuffer.resize(PROCESS_BUFFER_SIZE, 0.0f);
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
					V2F32 origin = box->computedOffset;
					F32 width = box->computedSize.x;
					F32 height = box->computedSize.y;
					for (size_t i = 0; i < osc.waveformBuffer.size() - 1; ++i) {
						V2F32 points[2];
						points[0].x = origin.x + (i / (F32)osc.waveformBuffer.size()) * width;
						points[0].y = origin.y + height / 2 + osc.waveformBuffer[i] * height / 2;
						points[1].x = origin.x + ((i + 1) / (F32)osc.waveformBuffer.size()) * width;
						points[1].y = origin.y + height / 2 + osc.waveformBuffer[i + 1] * height / 2;
						comm.tessellator->ui_line_strip(points, 2, comm.renderZ, 2.0F, V4F32{ 1.0F, 1.0F, 1.0F, 1.0F }, Textures::simpleWhite.index, 0);
					}
				}
				return UI::ACTION_HANDLED;
				};
			contentBox.unsafeBox->userData[1] = UPtr(this);
		}
	}

	void updateWaveform(const double* data, U32 size) {
		size_t copySize = std::min(static_cast<size_t>(size), waveformBuffer.size());
		for (size_t i = 0; i < copySize; ++i) {
			waveformBuffer[i] = static_cast<float>(data[i]);
		}
	}

	void destroy() {

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
union NodeWidget {
	NodeWidgetHeader header;
	NodeWidgetInput input;
	NodeWidgetOutput output;
	NodeWidgetOscilloscope oscilloscope;
	NodeWidgetSamplerButton file_dialog_button;
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

	NodeGraph* parent;
	NodeHeader* prev;
	NodeHeader* next;
	NodeHeader* selectedPrev;
	NodeHeader* selectedNext;
	UI::BoxHandle uiBox;

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
		header.get_input(0)->eval();
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
struct NodeSine {
	NodeHeader header;
	static const U32 TIME_INPUT_IDX = 0;
	static const U32 FREQUENCY_INPUT_IDX = 1;

	void init() {
		header.init(NODE_SINE, "Sine"sa);
		header.add_widget()->output.init();
		header.add_widget()->input.init(0.0);
		header.add_widget()->input.init(0.0);
	}
	void process() {
		NodeIOValue& time = header.get_input(TIME_INPUT_IDX)->eval();
		NodeIOValue& frequency = header.get_input(FREQUENCY_INPUT_IDX)->eval();
		NodeIOValue& output = header.get_output(0)->value;
		NodeIOValue* inputs[]{ &time, &frequency };
		NodeIOValue* outputs[]{ &output };
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
struct NodeMathOp {
	NodeHeader header;
	MathOp op;

	void init() {
		header.init(NODE_MATH, "Math"sa);
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
				_mm256_store_pd(output.buffer + i, _mm256_sub_pd(x, _mm256_mul_pd(_mm256_round_pd(_mm256_div_pd(x, y), _MM_FROUND_TO_ZERO | _MM_FROUND_NO_EXC), y)));
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
		print("does it get here ? ");
		NodeIOValue& input = header.get_input(TIME_INPUT_IDX)->eval();
		double* audioDataDouble = input.buffer;
		U32 bufferSize = input.bufferLength;
		NodeWidgetOscilloscope* oscWidget = reinterpret_cast<NodeWidgetOscilloscope*>(header.get_nth_of_type(NODE_WIDGET_OSCILLOSCOPE, 0));
		if (oscWidget) {
			oscWidget->updateWaveform(input.buffer, bufferSize);
			print("waveform updated.\n");
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
#include <iostream>
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
		NodeIOValue& time = header.get_input(TIME_INPUT_IDX)->eval();
		NodeIOValue& output = header.get_output(0)->value;
		for (U32 i = 0; i < output.bufferLength; i++) {
			F64 timeVal = time.buffer[i] * button.sampleRate; // should resample probably
			while (timeVal >= button.numSamples) {
				timeVal -= button.numSamples;
			}
			while (timeVal < 0) {
				timeVal += button.numSamples;
			}
			output.buffer[i] = button.audioData[U64(timeVal)];
		}
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
			inputs.push_back(&reinterpret_cast<NodeWidgetInput*>(widget)->eval());
		} else if (widget->type == NODE_WIDGET_OUTPUT) {
			outputs.push_back(&reinterpret_cast<NodeWidgetOutput*>(widget)->value);
		}
	}
	make_node_io_consistent(inputs.data, inputs.size, outputs.data, outputs.size);

	switch (node->type) {
	case NODE_TIME_IN:
		reinterpret_cast<NodeTimeIn*>(node)->process();
		break;
	case NODE_CHANNEL_OUT:
		reinterpret_cast<NodeChannelOut*>(node)->process();
		break;
	case NODE_SINE:
		reinterpret_cast<NodeSine*>(node)->process();
		break;
	case NODE_MATH:
		reinterpret_cast<NodeMathOp*>(node)->process();
		break;
	case NODE_OSCILLOSCOPE:
		reinterpret_cast<NodeOscilloscope*>(node)->process();
		break;
	case NODE_SAMPLER:
		reinterpret_cast<NodeSampler*>(node)->process();
		break;
	default:
		break;
	}
	
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
		str_a(StrA{ title, titleLength });
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