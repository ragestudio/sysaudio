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
napi_ref sharedBufferRef = nullptr;

void ReleaseCaptureCallback() {
	if (audioCaptureDataCallback) {
		printf("[SysAudio] Addon::ReleaseCaptureCallback > releasing tsfn\n");
		audioCaptureDataCallback.Release();
		audioCaptureDataCallback = nullptr;
	}
}

void onAudioDataCallbackFn(Napi::Env env, Napi::Function jsCb, AudioFrame *frame) {
	if (env != nullptr && jsCb != nullptr) {
		Napi::Buffer<uint8_t> buffer = Napi::Buffer<uint8_t>::Copy(env, frame->buff, frame->size);

		Napi::Object formatObj = Napi::Object::New(env);
		formatObj.Set("sampleRate", Napi::Number::New(env, frame->format->sampleRate));
		formatObj.Set("channels", Napi::Number::New(env, frame->format->channels));
		formatObj.Set("bitsPerSample", Napi::Number::New(env, frame->format->bitsPerSample));

		jsCb.Call({ buffer, formatObj });
	}

	if (frame) {
		delete[] frame->buff;
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

	InitializeParams params;
#ifdef __linux
	params.excluded_pid = initializeParams.Get("excluded_pid").As<Napi::Number>().Int32Value();
	params.node_name = initializeParams.Get("node_name").ToString();
	params.device_app_name = initializeParams.Get("device_app_name").ToString();
	params.device_app_id = initializeParams.Get("device_app_id").ToString();
	params.device_app_icon_name = initializeParams.Get("device_app_icon_name").ToString();
#endif
#ifdef _WIN32
	params.excluded_pid = static_cast<DWORD>(initializeParams.Get("excluded_pid").As<Napi::Number>().Uint32Value());
#endif

	engine.Initialize(params);

	return Napi::Boolean::New(env, true);
}

Napi::Value StartCapture(const Napi::CallbackInfo &info) {
	Napi::Env env = info.Env();

	if (info.Length() < 1 || !info[0].IsNumber()) {
		Napi::TypeError::New(env, "Invalid arguments").ThrowAsJavaScriptException();
		return env.Undefined();
	}

	int pid = info[0].As<Napi::Number>().Int32Value();

	// the callback is optional when a shared buffer is attached
	ReleaseCaptureCallback();

	if (info.Length() >= 2 && info[1].IsFunction()) {
		audioCaptureDataCallback = Napi::ThreadSafeFunction::New(
			env,
			info[1].As<Napi::Function>(),
			"audioCaptureDataCallback",
			512,
			1
		);
	}

	AudioDataCallback onAudioData = nullptr;

	if (audioCaptureDataCallback) {
		onAudioData = [](AudioFrame *frame) {
			napi_status status = audioCaptureDataCallback.NonBlockingCall(frame, onAudioDataCallbackFn);

			if (status != napi_ok) {
				// queue full or tsfn closed, cleanup the frame ourselves
				delete[] frame->buff;
				delete frame->format;
				delete frame;
			}
		};
	}

	printf("[SysAudio] Addon::StartCapture > starting capture\n");
	engine.StartCapture(pid, onAudioData);

	return Napi::Boolean::New(env, true);
}

Napi::Value StopCapture(const Napi::CallbackInfo &info) {
	printf("[SysAudio] Addon::StopCapture > stopping capture\n");

	engine.StopCapture();
	ReleaseCaptureCallback();

	printf("[SysAudio] Addon::StopCapture > done\n");
	return info.Env().Undefined();
}

Napi::Value AttachSharedBuffer(const Napi::CallbackInfo &info) {
	Napi::Env env = info.Env();

	if (info.Length() < 1 || !info[0].IsTypedArray()) {
		Napi::TypeError::New(env, "Expected a Uint8Array view of a SharedArrayBuffer").ThrowAsJavaScriptException();
		return env.Undefined();
	}

	Napi::TypedArray typed = info[0].As<Napi::TypedArray>();
	Napi::ArrayBuffer buffer = typed.ArrayBuffer();

	uint8_t *data = static_cast<uint8_t *>(buffer.Data()) + typed.ByteOffset();
	size_t length = typed.ByteLength();

	if (!data || length == 0) {
		Napi::TypeError::New(env, "Shared buffer is empty").ThrowAsJavaScriptException();
		return env.Undefined();
	}

	if (sharedBufferRef) {
		napi_delete_reference(env, sharedBufferRef);
		sharedBufferRef = nullptr;
	}

	engine.AttachSharedBuffer(data, length);

	napi_create_reference(env, info[0], 1, &sharedBufferRef);

	return Napi::Boolean::New(env, true);
}

Napi::Value DetachSharedBuffer(const Napi::CallbackInfo &info) {
	Napi::Env env = info.Env();

	engine.DetachSharedBuffer();

	if (sharedBufferRef) {
		napi_delete_reference(env, sharedBufferRef);
		sharedBufferRef = nullptr;
	}

	return env.Undefined();
}

Napi::Value GetFormat(const Napi::CallbackInfo &info) {
	Napi::Env env = info.Env();

	AudioFormat format = engine.GetFormat();

	Napi::Object formatObj = Napi::Object::New(env);
	formatObj.Set("sampleRate", Napi::Number::New(env, format.sampleRate));
	formatObj.Set("channels", Napi::Number::New(env, format.channels));
	formatObj.Set("bitsPerSample", Napi::Number::New(env, format.bitsPerSample));
	formatObj.Set("format", Napi::Number::New(env, format.format));

	return formatObj;
}

Napi::Value OutputAudio(const Napi::CallbackInfo &info) {
#ifdef __linux
	Napi::Env env = info.Env();

	if (info.Length() < 2 || !info[0].IsBuffer() || !info[1].IsObject()) {
		Napi::TypeError::New(env, "Invalid arguments").ThrowAsJavaScriptException();
		return env.Undefined();
	}

	Napi::Object formatObj = info[1].As<Napi::Object>();

	AudioFormat format = {};
	format.channels = formatObj.Get("channels").As<Napi::Number>().Int32Value();
	format.sampleRate = formatObj.Get("sampleRate").As<Napi::Number>().Int32Value();
	format.bitsPerSample = formatObj.Get("bitsPerSample").As<Napi::Number>().Int32Value();

	AudioFrame frame = {};
	frame.buff = info[0].As<Napi::Buffer<uint8_t>>().Data();
	frame.size = info[0].As<Napi::Buffer<uint8_t>>().Length();
	frame.format = &format;

	engine.Output(&frame);
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
	exports.Set("attach_shared_buffer", Napi::Function::New(env, AttachSharedBuffer));
	exports.Set("detach_shared_buffer", Napi::Function::New(env, DetachSharedBuffer));
	exports.Set("get_format", Napi::Function::New(env, GetFormat));

#ifdef __linux
	exports.Set("output_supported", Napi::Boolean::New(env, true));
#else
	exports.Set("output_supported", Napi::Boolean::New(env, false));
#endif
	return exports;
}

NODE_API_MODULE(sysaudio, Init)
