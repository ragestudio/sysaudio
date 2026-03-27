#include <napi.h>

#include <cstdio>

#include "types.hpp"

#ifdef _WIN32
#define OS "win32"
#define IS_WINDOWS
#include "win/capture.hpp"
#endif

#ifdef __linux
#define OS "linux"
#include "linux/engine.hpp"
#define IS_LINUX
#endif

#ifdef _WIN32
WasapiCapture engine;
#endif

#ifdef __linux
SysAudioLinux engine;
#endif

Napi::ThreadSafeFunction audioCaptureDataCallback;

void onAudioDataCallbackFn(Napi::Env env, Napi::Function jsCb, AudioFrame *frame) {
	if (env != nullptr && jsCb != nullptr) {
		Napi::Buffer<uint8_t> buffer = Napi::Buffer<uint8_t>::Copy(env, frame->buff, frame->size);

		Napi::Object formatObj = Napi::Object::New(env);
		formatObj.Set("sampleRate", Napi::Number::New(env, frame->format->sampleRate));
		formatObj.Set("channels", Napi::Number::New(env, frame->format->channels));
		formatObj.Set("bitsPerSample", Napi::Number::New(env, frame->format->bitsPerSample));

		// printf("Frame size: %zu\n", frame->size);
		// printf("Frame buffer: %p\n", frame->buff);

		jsCb.Call({ buffer, formatObj });
	}

	if (frame) {
		delete frame->format;
		delete frame;
	}
};

Napi::Value Initialize(const Napi::CallbackInfo &info) {
	Napi::Env env = info.Env();

	if (info.Length() < 1 || !info[0].IsObject()) {
		Napi::TypeError::New(env, "Invalid arguments").ThrowAsJavaScriptException();
		return env.Undefined();
	}

	Napi::Object initializeParams = info[0].As<Napi::Object>();

	InitializeParams params = {
		.excluded_pid = initializeParams.Get("excluded_pid").As<Napi::Number>().Int32Value(),
		.node_name = initializeParams.Get("node_name").ToString(),
		.device_app_name = initializeParams.Get("device_app_name").ToString(),
		.device_app_id = initializeParams.Get("device_app_id").ToString(),
		.device_app_icon_name = initializeParams.Get("device_app_icon_name").ToString(),
	};

	engine.Initialize(params);

	return Napi::Boolean::New(env, true);
}

Napi::Value StartCapture(const Napi::CallbackInfo &info) {
	Napi::Env env = info.Env();

	if (info.Length() < 2 || !info[0].IsNumber() || !info[1].IsFunction()) {
		Napi::TypeError::New(env, "Invalid arguments").ThrowAsJavaScriptException();
		return env.Undefined();
	}

	int pid = info[0].As<Napi::Number>().Int32Value();

#ifdef _WIN32
	pid = static_cast<DWORD>(pid);
#endif

#ifdef __linux
	pid = static_cast<pid_t>(pid);
#endif

	audioCaptureDataCallback = Napi::ThreadSafeFunction::New(
		env,
		info[1].As<Napi::Function>(),
		"audioCaptureDataCallback",
		10,	 // maximum queue size to prevent accumulation
		1
	);

	auto onAudioData = [](AudioFrame *frame) {
		if (audioCaptureDataCallback) {
			audioCaptureDataCallback.NonBlockingCall(frame, onAudioDataCallbackFn);
		}
	};

	printf("[SysAudio] Addon::StartCapture > starting capture\n");
	engine.StartCapture(pid, onAudioData);

	return Napi::Boolean::New(env, true);
}

Napi::Value StopCapture(const Napi::CallbackInfo &info) {
	printf("[SysAudio] Addon::StopCapture > stopping capture\n");

	engine.StopCapture();

	if (audioCaptureDataCallback) {
		printf("[SysAudio] Addon::StopCapture > releasing tsfn\n");
		audioCaptureDataCallback.Release();
		audioCaptureDataCallback = nullptr;
	}

	printf("[SysAudio] Addon::StopCapture > done\n");
	return info.Env().Undefined();
}

Napi::Value OutputAudio(const Napi::CallbackInfo &info) {
#ifdef __linux
	Napi::Env env = info.Env();

	if (info.Length() < 2 || !info[0].IsBuffer() || !info[1].IsObject()) {
		Napi::TypeError::New(env, "Invalid arguments").ThrowAsJavaScriptException();
		return env.Undefined();
	}

	Napi::Object formatObj = info[1].As<Napi::Object>();

	AudioFormat *format = new AudioFormat{};

	format->channels = formatObj.Get("channels").As<Napi::Number>().Int32Value();
	format->sampleRate = formatObj.Get("sampleRate").As<Napi::Number>().Int32Value();
	format->bitsPerSample = formatObj.Get("bitsPerSample").As<Napi::Number>().Int32Value();

	AudioFrame *frame = new AudioFrame{};

	frame->buff = info[0].As<Napi::Buffer<uint8_t>>().Data();
	frame->size = info[0].As<Napi::Buffer<uint8_t>>().Length();
	frame->format = format;

	engine.Output(frame);
	return env.Undefined();
#else
	Napi::Error::New(info.Env(), "OutputAudio is only supported on Linux").ThrowAsJavaScriptException();
	return info.Env().Undefined();
#endif
}

Napi::Value StopEngine(const Napi::CallbackInfo &info) {
	printf("[SysAudio] Addon::Stop > stopping engine\n");

	engine.Stop();

	printf("[SysAudio] Addon::Stop > done\n");
	return info.Env().Undefined();
}

Napi::Object Init(Napi::Env env, Napi::Object exports) {
	exports.Set("initialize", Napi::Function::New(env, Initialize));
	exports.Set("start_capture", Napi::Function::New(env, StartCapture));
	exports.Set("stop_capture", Napi::Function::New(env, StopCapture));
	exports.Set("stop", Napi::Function::New(env, StopEngine));
	exports.Set("output", Napi::Function::New(env, OutputAudio));

#ifdef __linux
	exports.Set("output_supported", Napi::Boolean::New(env, true));
#else
	exports.Set("output_supported", Napi::Boolean::New(env, false));
#endif
	return exports;
}

NODE_API_MODULE(sysaudio, Init)
