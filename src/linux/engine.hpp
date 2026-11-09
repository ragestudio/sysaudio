#ifndef SYSAUDIO_LINUX_ENGINE_HPP
#define SYSAUDIO_LINUX_ENGINE_HPP

#include <pipewire/core.h>
#include <pipewire/device.h>
#include <pipewire/keys.h>
#include <pipewire/main-loop.h>
#include <pipewire/pipewire.h>
#include <pipewire/properties.h>
#include <pipewire/stream.h>
#include <pipewire/thread-loop.h>
#include <spa/param/audio/format-utils.h>
#include <spa/utils/hook.h>
#include <sys/types.h>

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "../backends/audio_backend.hpp"
#include "../types.hpp"

// single pipewire engine: owns the thread loop, streams, router and formats
class SysAudioLinux : public AudioBackend {
   public:
	SysAudioLinux();
	~SysAudioLinux();

	// disable copying
	SysAudioLinux(const SysAudioLinux &) = delete;
	SysAudioLinux &operator=(const SysAudioLinux &) = delete;

	bool Initialize(const InitializeParams &params) override;
	void Stop() override;

	bool StartCapture(int exclude_pid, AudioDataCallback callback) override;
	void StopCapture() override;

	void Output(AudioFrame *frame) override;

	AudioFormat GetFormat() const override;

   private:
	struct PendingPort {
		uint32_t node_id;
		uint32_t port_id;
		std::string channel;
	};

	struct ActiveLink {
		pw_proxy *link;
		uint32_t node_id;
		uint32_t port_id;
		std::string channel;
	};

	// stream setup
	bool create_input_node();
	bool create_output_node();
	bool stop_capture(bool preserve_state = true);

	// stream processing
	void process_input_audio();
	void process_output_audio();
	void input_param_changed(uint32_t id, const struct spa_pod *param);
	void input_state_changes(enum pw_stream_state old, enum pw_stream_state state, const char *error);
	void output_state_changes(enum pw_stream_state old, enum pw_stream_state state, const char *error);

	// registry router
	void handle_registry_global(uint32_t id, const char *type, const struct spa_dict *props);
	void handle_registry_global_remove(uint32_t id);
	void try_link_ports();

	// pipewire C callbacks
	static void registry_global_cb(
		void *app,
		uint32_t id,
		uint32_t permissions,
		const char *type,
		uint32_t version,
		const struct spa_dict *props
	);
	static void registry_global_remove_cb(void *app, uint32_t id);

	static void input_process_cb(void *app);
	static void input_param_changed_cb(void *app, uint32_t id, const struct spa_pod *param);
	static void input_state_changes_cb(
		void *app,
		enum pw_stream_state old,
		enum pw_stream_state state,
		const char *error
	);

	static void output_process_cb(void *app);
	static void output_state_changes_cb(
		void *app,
		enum pw_stream_state old,
		enum pw_stream_state state,
		const char *error
	);

	static const struct pw_stream_events input_stream_events;
	static const struct pw_stream_events output_stream_events;
	static const struct pw_registry_events registry_events;

	// capture state
	std::atomic<bool> initialized;
	std::atomic<bool> is_capturing;
	pid_t excluded_pid;
	std::mutex callback_mutex;

	// pipewire objects
	pw_thread_loop *thread_loop;
	pw_loop *loop;

	pw_context *context;
	pw_core *core;
	pw_registry *registry;
	spa_hook registry_listener;

	// capture stream
	pw_stream *capture_stream;
	uint32_t capture_stream_node_id;

	// output stream
	pw_stream *output_stream;
	uint32_t output_stream_node_id;

	// router
	std::unordered_map<uint32_t, pid_t> target_nodes;  // node_id -> pid
	std::unordered_map<std::string, uint32_t> stream_ports;
	std::vector<PendingPort> pending_ports;
	std::unordered_map<uint32_t, std::vector<ActiveLink>> active_links;

	// audio format state
	uint32_t current_format;
	uint32_t current_rate;
	uint32_t current_channels;

	// initialization parameters
	std::string node_name;
	std::string output_node_name;
	std::string app_name;
	std::string app_id;
	std::string app_icon_name;

	// process that owns the streams (the electron renderer running the preload)
	pid_t self_pid;

	// user callback
	AudioDataCallback data_callback;
};

#endif	// SYSAUDIO_LINUX_ENGINE_HPP
