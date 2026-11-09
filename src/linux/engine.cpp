#include <unistd.h>

#include <chrono>
#include <cstdio>
#include <cstring>
#include <thread>

#include "engine.hpp"
#include "utils.hpp"

SysAudioLinux::SysAudioLinux()
	: initialized(false), is_capturing(false), excluded_pid(0), thread_loop(nullptr), loop(nullptr), context(nullptr), core(nullptr), registry(nullptr), capture_stream(nullptr), capture_stream_node_id(0), output_stream(nullptr), output_stream_node_id(0), current_format(SPA_AUDIO_FORMAT_UNKNOWN), current_rate(0), current_channels(0), node_name("comty-sysaudio"), output_node_name("comty-sysaudio-output"), app_name("Comty"), app_id("comty.desktop"), app_icon_name("comty"), self_pid(0), data_callback(nullptr) {
	memset(&registry_listener, 0, sizeof(registry_listener));
}

SysAudioLinux::~SysAudioLinux() {
	Stop();
}

bool SysAudioLinux::Initialize(const InitializeParams &params) {
	if (initialized) {
		return false;
	}

	printf("[SysAudio] Initialized with params:\n");
	printf("  excluded_pid: %d\n", params.excluded_pid);
	printf("  node_name: %s\n", params.node_name.c_str());
	printf("  device_app_name: %s\n", params.device_app_name.c_str());
	printf("  device_app_id: %s\n", params.device_app_id.c_str());
	printf("  device_app_icon_name: %s\n", params.device_app_icon_name.c_str());

	printf("Initializing sysaudio engine...\n");

	// store initialization parameters
	if (!params.node_name.empty()) {
		node_name = params.node_name;
	}

	// keep the capture and playback nodes uniquely named so the router can
	// tell them apart from regular apps
	output_node_name = node_name + "-output";
	self_pid = getpid();

	if (!params.device_app_name.empty()) {
		app_name = params.device_app_name;
	}
	if (!params.device_app_id.empty()) {
		app_id = params.device_app_id;
	}
	if (!params.device_app_icon_name.empty()) {
		app_icon_name = params.device_app_icon_name;
	}

	// initialize pipewire
	pw_init(nullptr, nullptr);

	thread_loop = pw_thread_loop_new("sysaudio-engine", nullptr);
	if (!thread_loop) {
		fprintf(stderr, "Failed to create PipeWire thread loop\n");
		return false;
	}

	loop = pw_thread_loop_get_loop(thread_loop);
	if (!loop) {
		fprintf(stderr, "Failed to get PipeWire loop\n");
		Stop();
		return false;
	}

	// create context
	context = pw_context_new(loop, nullptr, 0);
	if (!context) {
		fprintf(stderr, "Failed to create PipeWire context\n");
		Stop();
		return false;
	}

	// connect core
	core = pw_context_connect(context, nullptr, 0);
	if (!core) {
		fprintf(stderr, "Failed to connect PipeWire context\n");
		Stop();
		return false;
	}

	// get registry
	registry = pw_core_get_registry(core, PW_VERSION_REGISTRY, 0);
	if (!registry) {
		fprintf(stderr, "Failed to get PipeWire registry\n");
		Stop();
		return false;
	}

	// add registry listener
	pw_registry_add_listener(registry, &registry_listener, &registry_events, this);

	if (!create_output_node()) {
		fprintf(stderr, "Failed to create PipeWire output stream node\n");
		Stop();
		return false;
	}

	if (pw_thread_loop_start(thread_loop) < 0) {
		fprintf(stderr, "Failed to start PipeWire thread\n");
		pw_thread_loop_destroy(thread_loop);
		thread_loop = nullptr;
		return false;
	}

	// brief wait for initialization
	std::this_thread::sleep_for(std::chrono::milliseconds(10));

	printf("SysAudio engine initialized\n");

	initialized = true;
	return true;
}

void SysAudioLinux::Stop() {
	if (!thread_loop && !capture_stream && !output_stream && !registry && !core && !context) {
		return;
	}

	// stop_capture locks the thread loop internally, so it must run before
	// the cleanup block takes the lock
	if (capture_stream) {
		stop_capture(false);
		target_nodes.clear();
		pending_ports.clear();
	}

	if (thread_loop) {
		pw_thread_loop_lock(thread_loop);
	}

	if (output_stream) {
		printf("Disconnecting output stream\n");
		pw_stream_disconnect(output_stream);

		printf("Destroying output stream\n");
		pw_stream_destroy(output_stream);

		output_stream = nullptr;
		output_stream_node_id = 0;
	}

	if (registry) {
		printf("Destroying registry\n");
		spa_hook_remove(&registry_listener);
		pw_proxy_destroy((struct pw_proxy *)registry);
		registry = nullptr;
	}

	if (core) {
		printf("Destroying core\n");
		pw_core_disconnect(core);
		core = nullptr;
	}

	if (context) {
		printf("Destroying context\n");
		pw_context_destroy(context);
		context = nullptr;
	}

	if (data_callback) {
		std::lock_guard<std::mutex> lock(callback_mutex);
		data_callback = nullptr;
	}

	if (thread_loop) {
		printf("Stopping thread loop\n");
		pw_thread_loop_unlock(thread_loop);
		pw_thread_loop_stop(thread_loop);
		pw_thread_loop_destroy(thread_loop);
		thread_loop = nullptr;
	}

	pw_deinit();

	is_capturing = false;
	shared_buffer.set_capture_active(false);
	initialized = false;
	printf("SysAudio engine stopped\n");
}

void SysAudioLinux::StopCapture() {
	stop_capture();
}

AudioFormat SysAudioLinux::GetFormat() const {
	AudioFormat format = {};

	format.sampleRate = current_rate;
	format.channels = current_channels;
	format.format = current_format;

	if (current_format == SPA_AUDIO_FORMAT_UNKNOWN || current_rate == 0 || current_channels == 0) {
		format.bitsPerSample = 0;
	} else {
		format.bitsPerSample = sysaudio::utils::get_bytes_per_sample(static_cast<spa_audio_format>(current_format)) * 8;
	}

	return format;
}
