#pragma once
#include "VK.h"
#include "DynamicVertexBuffer.h"
#include "TextRenderer.h"
#include "WASAPIInterface.h"
#include "Nodes.h"
#include "NodeUI.h"

namespace DAWdle {

U64 frameNumber;
U64 prevFrameTime;
U64 frameTime;
F64 deltaTime;
F64 totalTime;
// F32 cannot be used for total audio time. The precision is insufficient and results in audible artifacts after only a handful of seconds
F64 audioPlaybackTime;
U32 audioBufferCount;
F32 audioBuffer[Nodes::PROCESS_BUFFER_SIZE];

Nodes::NodeGraph primaryGraph;

HANDLE audioThread;
B32 audioThreadShouldShutdown;

void fill_audio_buffer(F32* buffer, U32 numSamples, U32 numChannels, F32 timeAmount) {
	UI::modificationLock.lock_read();
	U32 currentSample = 0;
	while (currentSample < numSamples) {
		if (audioBufferCount == 0) {
			F64 timeBuffer[ARRAY_COUNT(audioBuffer)];
			for (U32 i = 0; i < ARRAY_COUNT(audioBuffer); i++) {
				timeBuffer[i] = audioPlaybackTime + F64(currentSample + i) / F64(numSamples) * F64(timeAmount);
			}
			primaryGraph.generate_output(audioBuffer, timeBuffer);
			audioBufferCount = ARRAY_COUNT(audioBuffer);
		}
		U32 samplesToGenerate = min(audioBufferCount, numSamples - currentSample);
		F32* audioPtr = &audioBuffer[ARRAY_COUNT(audioBuffer) - audioBufferCount];
		for (U32 i = 0; i < samplesToGenerate; i++) {
			for (U32 j = 0; j < numChannels; j++) {
				*buffer++ = audioPtr[i];
			}
		}
		audioBufferCount -= samplesToGenerate;
		currentSample += samplesToGenerate;
	}
	audioPlaybackTime += timeAmount;
	UI::modificationLock.unlock_read();
}

DWORD WINAPI audio_thread_func(LPVOID) {
	audioPlaybackTime = 2048.0;
	WASAPIInterface::init_wasapi(fill_audio_buffer);
	while (!audioThreadShouldShutdown) {
		WASAPIInterface::do_audio();
	}
	return 0;
}

void keyboard_callback(Win32::Key key, Win32::ButtonState state) {
	V2F32 mousePos = Win32::get_mouse();
	UI::handle_keyboard_action(mousePos, key, state);
}
void mouse_callback(Win32::MouseButton button, Win32::MouseValue state) {
	V2F32 mousePos = Win32::get_mouse();
	UI::handle_mouse_action(mousePos, button, state);
}

void do_frame() {
	frameNumber++;
	LARGE_INTEGER perfCounter;
	if (!QueryPerformanceCounter(&perfCounter)) {
		abort("Could not get performance counter");
	}
	prevFrameTime = frameTime;
	frameTime = U64(perfCounter.QuadPart);
	deltaTime = F64(frameTime - prevFrameTime) / F64(performanceCounterTimerFrequency);
	totalTime += deltaTime;

	V2F32 mousePos = Win32::get_mouse();
	V2F32 mouseDelta = Win32::get_delta_mouse();
	UI::handle_mouse_update(mousePos, mouseDelta);

	VK::FrameBeginResult beginAction = VK::begin_frame();
	if (beginAction == VK::FRAME_BEGIN_RESULT_TRY_AGAIN) {
		beginAction = VK::begin_frame();
	}
	if (beginAction == VK::FRAME_BEGIN_RESULT_CONTINUE) {
		UI::layout_boxes(VK::desktopSwapchainData.width, VK::desktopSwapchainData.height);

		VkViewport viewport;
		viewport.x = 0;
		viewport.y = 0;
		viewport.width = F32(VK::mainFramebuffer.framebufferWidth);
		viewport.height = F32(VK::mainFramebuffer.framebufferHeight);
		viewport.minDepth = 0.0F;
		viewport.maxDepth = 1.0F;
		VK::vkCmdSetViewport(VK::graphicsCommandBuffer, 0, 1, &viewport);
		VkRect2D scissor{};
		scissor.offset.x = 0;
		scissor.offset.y = 0;
		scissor.extent.width = VK::mainFramebuffer.framebufferWidth;
		scissor.extent.height = VK::mainFramebuffer.framebufferHeight;
		VK::vkCmdSetScissor(VK::graphicsCommandBuffer, 0, 1, &scissor);

		VkRenderPassBeginInfo renderPassBeginInfo{ VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
		renderPassBeginInfo.renderPass = VK::mainRenderPass;
		renderPassBeginInfo.framebuffer = VK::mainFramebuffer.framebuffer;
		renderPassBeginInfo.renderArea = VkRect2D{ VkOffset2D{ 0, 0 }, VkExtent2D{ VK::mainFramebuffer.framebufferWidth, VK::mainFramebuffer.framebufferHeight } };
		renderPassBeginInfo.clearValueCount = 2;
		VkClearValue clearValues[2];
		clearValues[0].color.float32[0] = 0.0F;
		clearValues[0].color.float32[1] = 0.0F;
		clearValues[0].color.float32[2] = 0.0F;
		clearValues[0].color.float32[3] = 0.0F;
		clearValues[1].depthStencil.depth = 0.0F;
		clearValues[1].depthStencil.stencil = 0;
		renderPassBeginInfo.pClearValues = clearValues;
		VK::vkCmdBeginRenderPass(VK::graphicsCommandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

		UI::draw();
		DynamicVertexBuffer::get_tessellator().draw();

		VK::vkCmdEndRenderPass(VK::graphicsCommandBuffer);

		VK::end_frame();
	} else {
		Sleep(1);
	}
}

U32 run_dawdle() {
	//MSDFGenerator::generate_msdf_font("./resources/font.svg"sa, "./resources/textures/font.ddf"sa, 64, 64, PX_TO_MILLIMETER(6.0F), PX_TO_MILLIMETER(12.0F), 12.0F, false);
	//return 0;
	audioThread = CreateThread(NULL, 64 * KILOBYTE, audio_thread_func, NULL, 0, NULL);
	if (audioThread == NULL) {
		DWORD err = GetLastError();
		print("Failed to create audio thread, code: ");
		println_integer(err);
		return EXIT_FAILURE;
	}
	
	LOG_TIME("Total Init Time: ") {
		LARGE_INTEGER perfCounter;
		if (!QueryPerformanceCounter(&perfCounter)) {
			abort("Could not get performanceCounter");
		}
		frameTime = prevFrameTime = U64(perfCounter.QuadPart);

		if (!Win32::init(1920 / 2, 1080 / 2, do_frame, keyboard_callback, mouse_callback)) {
			return EXIT_FAILURE;
		}
		PNG::init_loader();
		LOG_TIME("VK Init Time: ") {
			VK::init_vulkan();
		}
		LOG_TIME("Asset Load Time: ") {
			VK::load_pipelines_and_descriptors();
			Textures::load_all();
			VK::finish_startup();
		}

		UI::init_ui();
		
		UI::modificationLock.lock_write();
		primaryGraph.init();
		NodeUI::init(&primaryGraph);
		UI::modificationLock.unlock_write();
	}
	
	Win32::show_window();

	char buf[1024];
	U32 bufLen = 1024;
	Win32::get_clipboard(buf, &bufLen);
	println(StrA{ buf, bufLen });

	Win32::set_clipboard("Quack"sa);

	while (!Win32::windowShouldClose) {
		Win32::poll_events();
		do_frame();
	}

	LOG_TIME("Total Shutdown Time: ") {
		audioThreadShouldShutdown = true;
		if (WaitForSingleObject(audioThread, INFINITE) == WAIT_FAILED) {
			DWORD err = GetLastError();
			print("Failed to join audio thread, code: ");
			println_integer(err);
		} else {
			CloseHandle(audioThread);
		}
		CHK_VK(VK::vkDeviceWaitIdle(VK::logicalDevice));
		LOG_TIME("UI Shutdown Time: ") {
			UI::destroy_ui();
		}
		LOG_TIME("VK Shutdown Time: ") {
			VK::end_vulkan();
		}
		LOG_TIME("Win32 Shutdown Time: ") {
			Win32::destroy();
		}
	}
	

	return EXIT_SUCCESS;
}

}