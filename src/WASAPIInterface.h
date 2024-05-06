#pragma once
#define INITGUID
#include <Audioclient.h>
#include <Audiopolicy.h>
#include <Mmdeviceapi.h>
#include <mmreg.h>
#include <avrt.h>

#define CHK_WASAPI(cmd) {\
		HRESULT result = (cmd);\
		if (result != S_OK) {\
			WASAPIInterface::wasapi_failure(result);\
		}\
	}\

namespace WASAPIInterface {

#define AUDIO_FORMATS \
	X(AUDIO_FORMAT_8BIT_INTEGER_44_1_KHZ, 8, false, 44100)\
	X(AUDIO_FORMAT_8BIT_INTEGER_48_KHZ, 8, false, 48000)\
	X(AUDIO_FORMAT_8BIT_INTEGER_96_KHZ, 8, false, 96000)\
	X(AUDIO_FORMAT_16BIT_INTEGER_44_1_KHZ, 16, false, 44100)\
	X(AUDIO_FORMAT_16BIT_INTEGER_48_KHZ, 16, false, 48000)\
	X(AUDIO_FORMAT_16BIT_INTEGER_96_KHZ, 16, false, 96000)\
	X(AUDIO_FORMAT_24BIT_INTEGER_44_1_KHZ, 24, false, 44100)\
	X(AUDIO_FORMAT_24BIT_INTEGER_48_KHZ, 24, false, 48000)\
	X(AUDIO_FORMAT_24BIT_INTEGER_96_KHZ, 24, false, 96000)\
	X(AUDIO_FORMAT_32BIT_INTEGER_44_1_KHZ, 32, false, 44100)\
	X(AUDIO_FORMAT_32BIT_INTEGER_48_KHZ, 32, false, 48000)\
	X(AUDIO_FORMAT_32BIT_INTEGER_96_KHZ, 32, false, 96000)\
	X(AUDIO_FORMAT_32BIT_IEEE_FLOAT_44_1_KHZ, 32, true, 44100)\
	X(AUDIO_FORMAT_32BIT_IEEE_FLOAT_48_KHZ, 32, true, 48000)\
	X(AUDIO_FORMAT_32BIT_IEEE_FLOAT_96_KHZ, 32, true, 96000)\


enum AudioFormat {
#define X(name, bitDepth, isFloat, frequencyHz) name,
	AUDIO_FORMATS
#undef X
	AUDIO_FORMAT_Count
};

HMODULE combaseDLL;
HMODULE oleDLL;
HMODULE avrtDLL;

decltype(CoInitialize)* pCoInitialize;
decltype(CoCreateInstance)* pCoCreateInstance;
decltype(CoTaskMemFree)* pCoTaskMemFree;
decltype(CLSID_MMDeviceEnumerator)* pCLSID_MMDeviceEnumerator;
decltype(IID_IMMDeviceEnumerator)* pIID_IMMDeviceEnumerator;
decltype(AvSetMmThreadCharacteristicsA)* pAvSetMmThreadCharacteristicsA;

IMMDeviceEnumerator* deviceEnumerator;
IMMDevice* device;
IAudioClient* audioClient;
IAudioRenderClient* audioRenderClient;
AudioFormat outputAudioFormat;
WAVEFORMATEXTENSIBLE outputWaveFormat;
U32 bufferFrames;
REFERENCE_TIME requestedBufferDuration;
F32* audioBuffer;

HANDLE wakeupTimer;

B32 (*audioBufferFillCallback)(F32* buffer, U32 numSamples, U32 numChannels, F32 timeAmount);

void wasapi_failure(HRESULT failureResult) {
	print("WASAPI function failed!\n");
	__debugbreak();
	ExitProcess(EXIT_FAILURE);
}

void load_dll_checked(HMODULE* result, const char* name) {
	*result = LoadLibraryA(name);
	if (*result == nullptr) {
		print(name);
		println(" failed to load, exiting");
		ExitProcess(EXIT_FAILURE);
	}
}

template<typename T>
void load_symbol_checked(T* result, HMODULE moduleToLoadFrom, const char* name) {
	*result = reinterpret_cast<T>(reinterpret_cast<void*>(GetProcAddress(moduleToLoadFrom, name)));
	if (*result == nullptr) {
		print(name);
		println(" failed to load, exiting");
		ExitProcess(EXIT_FAILURE);
	}
}

void load_audio_functions() {
	load_dll_checked(&oleDLL, "ole32.dll");
	load_symbol_checked(&pCoInitialize, oleDLL, "CoInitialize");

	load_dll_checked(&combaseDLL, "combase.dll");
	load_symbol_checked(&pCoCreateInstance, combaseDLL, "CoCreateInstance");
	load_symbol_checked(&pCoTaskMemFree, combaseDLL, "CoTaskMemFree");

	load_dll_checked(&avrtDLL, "avrt.dll");
	load_symbol_checked(&pAvSetMmThreadCharacteristicsA, avrtDLL, "AvSetMmThreadCharacteristicsA");
}

#define X(name, bitDepth, isFloat, frequencyHz) bitDepth,
const U32 AUDIO_FORMAT_BIT_DEPTH[]{
	AUDIO_FORMATS
	0
};
#undef X
#define X(name, bitDepth, isFloat, frequencyHz) isFloat,
const B32 AUDIO_FORMAT_IS_FLOAT[]{
	AUDIO_FORMATS
	false
};
#undef X
#define X(name, bitDepth, isFloat, frequencyHz) frequencyHz,
const U32 AUDIO_FORMAT_SAMPLE_RATE_HZ[]{
	AUDIO_FORMATS
	0
};
#undef X
#define X(name, bitDepth, isFloat, frequencyHz) #name,
const char* AUDIO_FORMAT_NAMES[]{
	AUDIO_FORMATS
	"AUDIO_FORMAT_COUNT"
};
#undef X

#undef AUDIO_FORMATS

AudioFormat audioFormatAttemptOrder[]{
	AUDIO_FORMAT_32BIT_IEEE_FLOAT_44_1_KHZ,
	AUDIO_FORMAT_32BIT_IEEE_FLOAT_48_KHZ,
	AUDIO_FORMAT_32BIT_IEEE_FLOAT_96_KHZ,
	AUDIO_FORMAT_32BIT_INTEGER_44_1_KHZ,
	AUDIO_FORMAT_32BIT_INTEGER_48_KHZ,
	AUDIO_FORMAT_32BIT_INTEGER_96_KHZ,
	AUDIO_FORMAT_24BIT_INTEGER_44_1_KHZ,
	AUDIO_FORMAT_24BIT_INTEGER_48_KHZ,
	AUDIO_FORMAT_24BIT_INTEGER_96_KHZ,
	AUDIO_FORMAT_16BIT_INTEGER_44_1_KHZ,
	AUDIO_FORMAT_16BIT_INTEGER_48_KHZ,
	AUDIO_FORMAT_16BIT_INTEGER_96_KHZ,
	AUDIO_FORMAT_8BIT_INTEGER_44_1_KHZ,
	AUDIO_FORMAT_8BIT_INTEGER_48_KHZ,
	AUDIO_FORMAT_8BIT_INTEGER_96_KHZ
};

void audio_format_to_waveformatextensible(WAVEFORMATEXTENSIBLE* waveFormat, AudioFormat format, U32 channelCount, U32 channelMask) {
	zero_memory(waveFormat, sizeof(WAVEFORMATEXTENSIBLE));
	waveFormat->Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
	waveFormat->Format.nChannels = WORD(channelCount);
	waveFormat->Format.nSamplesPerSec = AUDIO_FORMAT_SAMPLE_RATE_HZ[format];
	U32 bitDepth = AUDIO_FORMAT_BIT_DEPTH[format];
	waveFormat->Format.wBitsPerSample = WORD(bitDepth == 24 ? 32 : bitDepth);
	waveFormat->Format.nBlockAlign = WORD(next_power_of_two(U32(waveFormat->Format.nChannels * (waveFormat->Format.wBitsPerSample / 8))));
	waveFormat->Format.nAvgBytesPerSec = waveFormat->Format.nBlockAlign * waveFormat->Format.nSamplesPerSec;
	waveFormat->Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);

	waveFormat->Samples.wValidBitsPerSample = WORD(bitDepth);
	waveFormat->dwChannelMask = channelMask;
	waveFormat->SubFormat = AUDIO_FORMAT_IS_FLOAT[format] ? KSDATAFORMAT_SUBTYPE_IEEE_FLOAT : KSDATAFORMAT_SUBTYPE_PCM;
}

void encode_audio_to_final_buffer(void* outputBuffer, F32* inputBuffer, U32 numFrames, U32 numChannels, AudioFormat format) {
	U32 bitDepth = AUDIO_FORMAT_BIT_DEPTH[format];
	if (AUDIO_FORMAT_IS_FLOAT[format]) {
		F32* floatOutput = reinterpret_cast<F32*>(outputBuffer);
		for (U32 i = 0; i < numFrames * numChannels; i++) {
			floatOutput[i] = clamp(inputBuffer[i], -1.0F, 1.0F);
		}
	} else {
		F32 amplitude = F32((1 << bitDepth - 1) - 1);
		if (bitDepth <= 8) {
			U8* integerOutput = reinterpret_cast<U8*>(outputBuffer);
			for (U32 i = 0; i < numFrames * numChannels; i++) {
				integerOutput[i] = U8(U32(clamp(inputBuffer[i], -1.0F, 1.0F) * amplitude) << (8 - bitDepth));
			}
		} else if (bitDepth <= 16) {
			U16* integerOutput = reinterpret_cast<U16*>(outputBuffer);
			for (U32 i = 0; i < numFrames * numChannels; i++) {
				integerOutput[i] = U16(U32(clamp(inputBuffer[i], -1.0F, 1.0F) * amplitude) << (16 - bitDepth));
			}
		} else {
			U32* integerOutput = reinterpret_cast<U32*>(outputBuffer);
			for (U32 i = 0; i < numFrames * numChannels; i++) {
				integerOutput[i] = U32(clamp(inputBuffer[i], -1.0F, 1.0F) * amplitude) << (32 - bitDepth);
			}
		}
	}
}

void init_wasapi(B32 (*bufferFillCallback)(F32* buffer, U32 numSamples, U32 numChannels, F32 timeAmount)) {
	audioBufferFillCallback = bufferFillCallback;
	load_audio_functions();

	CHK_WASAPI(pCoInitialize(nullptr));

	CHK_WASAPI(pCoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), reinterpret_cast<void**>(&deviceEnumerator)));
	CHK_WASAPI(deviceEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device));
	CHK_WASAPI(device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(&audioClient)));

	/* Possibly use this to get the correct audio format for exclusive mode if we ever try to implement that
	WAVEFORMATEX* waveFormat2;
	WAVEFORMATEXTENSIBLE* waveformatExtensible2 = nullptr;
	if (exclusiveMode) {
		IPropertyStore* store;
		if (device->OpenPropertyStore(STGM_READ, &store) == S_OK) {
			PROPVARIANT prop;
			if (store->GetValue(PKEY_AudioEngine_DeviceFormat, &prop) == S_OK) {
				waveFormat2 = reinterpret_cast<WAVEFORMATEX*>(prop.blob.pBlobData);
				waveformatExtensible2 = reinterpret_cast<WAVEFORMATEXTENSIBLE*>(waveFormat2);
				if (audioClient->IsFormatSupported(AUDCLNT_SHAREMODE_EXCLUSIVE, waveFormat2, nullptr) == S_OK) {
					goto exclusiveModeOK;
				}
			}
		}
		exclusiveMode = false;
	exclusiveModeOK:;
		println("Using exclusive mode");
	}
	*/

	for (U32 i = 0; i < ARRAY_COUNT(audioFormatAttemptOrder); i++) {
		outputAudioFormat = audioFormatAttemptOrder[i];
		audio_format_to_waveformatextensible(&outputWaveFormat, outputAudioFormat, 2, SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT);
		WAVEFORMATEX* closestMatch;
		if (audioClient->IsFormatSupported(AUDCLNT_SHAREMODE_SHARED, &outputWaveFormat.Format, &closestMatch) == S_OK) {
			goto foundSuitableFormat;
		}
	}
	println("Failed to find suitable format for audio engine, exiting");
	ExitProcess(EXIT_FAILURE);
foundSuitableFormat:;
	print("Using audio format ");
	println(AUDIO_FORMAT_NAMES[outputAudioFormat]);

	//REFERENCE_TIME defaultPeriod, minPeriod;
	//CHK_WASAPI(audioClient->GetDevicePeriod(&defaultPeriod, &minPeriod));
	//U64 defaultPeriodMillis = U64(REFERENCE_TIME_TO_MICROSECOND(defaultPeriod));
	//U64 minPeriodMillis = U64(REFERENCE_TIME_TO_MICROSECOND(minPeriod));

	// Try to associate this thread with low latency audio in the hopes that it gets prioritized. It shouldn't matter if this fails.
	DWORD taskIndex = 0;
	(void) pAvSetMmThreadCharacteristicsA("Pro Audio", &taskIndex);

	requestedBufferDuration = MILLISECOND_TO_REFERENCE_TIME(20);
	CHK_WASAPI(audioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, requestedBufferDuration, 0, &outputWaveFormat.Format, nullptr));

	CHK_WASAPI(audioClient->GetBufferSize(&bufferFrames));
	CHK_WASAPI(audioClient->GetService(__uuidof(IAudioRenderClient), reinterpret_cast<void**>(&audioRenderClient)));

	audioBuffer = reinterpret_cast<F32*>(HeapAlloc(GetProcessHeap(), 0, bufferFrames * outputWaveFormat.Format.nChannels * sizeof(F32)));
	if (audioBuffer == nullptr) {
		abort("Failed to alloc audio buffer");
	}
	CHK_WASAPI(audioClient->Start());

	wakeupTimer = CreateWaitableTimerA(NULL, TRUE, NULL);
}

void shutdown_wasapi() {
	HeapFree(GetProcessHeap(), 0, audioBuffer);
	CHK_WASAPI(audioClient->Stop());

	audioRenderClient->Release();
	audioClient->Release();
	device->Release();
	deviceEnumerator->Release();
}

void do_audio() {
	static F32 time;
	REFERENCE_TIME actualDuration = REFERENCE_TIME(F64(requestedBufferDuration) * bufferFrames / outputWaveFormat.Format.nSamplesPerSec);
	REFERENCE_TIME sleepDuration = actualDuration / 2;
	if (wakeupTimer) {
		// Try to use the timer if possible. I think it's supposed to have higher precision than Sleep. Not sure, but I'll try to use it anyway.
		LARGE_INTEGER wakeupTime;
		wakeupTime.QuadPart = -sleepDuration;
		if (!SetWaitableTimer(wakeupTimer, &wakeupTime, 0, NULL, NULL, TRUE)) {
			Sleep(DWORD(REFERENCE_TIME_TO_MILLISECOND(sleepDuration)));
		} else {
			if (WaitForSingleObject(wakeupTimer, INFINITE) == WAIT_FAILED) {
				Sleep(DWORD(REFERENCE_TIME_TO_MILLISECOND(sleepDuration)));
			}
		}
	} else {
		Sleep(DWORD(REFERENCE_TIME_TO_MILLISECOND(sleepDuration)));
	}

	BYTE* finalBuffer;
	UINT32 paddingFrames;
	if (audioClient->GetCurrentPadding(&paddingFrames) != S_OK) {
		return;
	}
	U32 framesToFill = bufferFrames - paddingFrames;
	if (framesToFill == 0) {
		return;
	}
	if (audioRenderClient->GetBuffer(framesToFill, &finalBuffer) != S_OK) {
		return;
	}

	// Put data in buffer
	B32 hasAudio = audioBufferFillCallback(audioBuffer, framesToFill, outputWaveFormat.Format.nChannels, F32(framesToFill) / F32(AUDIO_FORMAT_SAMPLE_RATE_HZ[outputAudioFormat]));
	encode_audio_to_final_buffer(finalBuffer, audioBuffer, framesToFill, outputWaveFormat.Format.nChannels, outputAudioFormat);

	audioRenderClient->ReleaseBuffer(framesToFill, hasAudio ? 0 : AUDCLNT_BUFFERFLAGS_SILENT);
}

}

