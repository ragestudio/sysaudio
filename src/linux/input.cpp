#include <algorithm>
#include <cstdio>
#include <cstring>

#include "engine.hpp"
#include "utils.hpp"

const struct pw_stream_events SysAudioLinux::input_stream_events = {
	.version = PW_VERSION_STREAM_EVENTS,
	.destroy = nullptr,
	.state_changed = &SysAudioLinux::input_state_changes_cb,
	.control_info = nullptr,
	.io_changed = nullptr,
	.param_changed = &SysAudioLinux::input_param_changed_cb,
	.add_buffer = nullptr,
	.remove_buffer = nullptr,
	.process = &SysAudioLinux::input_process_cb,
	.drained = nullptr,
	.command = nullptr,
	.trigger_done = nullptr,
};

void SysAudioLinux::input_process_cb(void *app) {
	static_cast<SysAudioLinux *>(app)->process_input_audio();
}

void SysAudioLinux::input_param_changed_cb(void *app, uint32_t id, const struct spa_pod *param) {
	static_cast<SysAudioLinux *>(app)->input_param_changed(id, param);
}

void SysAudioLinux::input_state_changes_cb(void *app, enum pw_stream_state old, enum pw_stream_state state, const char *error) {
	static_cast<SysAudioLinux *>(app)->input_state_changes(old, state, error);
}

bool SysAudioLinux::create_input_node() {
	if (!core) {
		return false;
	}
	printf("Creating input node...\n");

	if (thread_loop) {
		pw_thread_loop_lock(thread_loop);
	}

	// create properties for the stream
	pw_properties *props = pw_properties_new(
		PW_KEY_MEDIA_TYPE, "Audio",
		PW_KEY_MEDIA_CATEGORY, "Capture",
		PW_KEY_MEDIA_ROLE, "Music",
		PW_KEY_NODE_NAME, node_name.c_str(),
		PW_KEY_NODE_DESCRIPTION, "System-wide audio capture",
		PW_KEY_APP_ID, app_id.c_str(),
		PW_KEY_APP_NAME, app_name.c_str(),
		PW_KEY_APP_ICON_NAME, app_icon_name.c_str(),
		PW_KEY_NODE_VIRTUAL, "true",
		PW_KEY_NODE_ALWAYS_PROCESS, "true",
		PW_KEY_NODE_LATENCY, "128/44100",
		PW_KEY_NODE_RATE, "1/44100",
		PW_KEY_NODE_LOCK_QUANTUM, "128/44100",
		nullptr
	);

	capture_stream = pw_stream_new_simple(
		loop,
		node_name.c_str(),
		props,
		&input_stream_events,
		this
	);

	if (!capture_stream) {
		return false;
	}

	uint8_t buffer[2048];

	const struct spa_pod *params[5];
	struct spa_pod_builder builder = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));

	int param_count = 0;

	// preferred format: S16LE, 44100Hz, stereo
	struct spa_audio_info_raw audio_info = SPA_AUDIO_INFO_RAW_INIT(
			.format = SPA_AUDIO_FORMAT_S16_LE,
			.flags = 0,
			.rate = 44100,
			.channels = 2,
			.position = { SPA_AUDIO_CHANNEL_FL, SPA_AUDIO_CHANNEL_FR }
	);
	params[param_count++] = spa_format_audio_raw_build(&builder, SPA_PARAM_EnumFormat, &audio_info);

	// alternative: S16LE, 48000Hz, stereo
	audio_info = SPA_AUDIO_INFO_RAW_INIT(
			.format = SPA_AUDIO_FORMAT_S16_LE,
			.flags = 0,
			.rate = 48000,
			.channels = 2,
			.position = { SPA_AUDIO_CHANNEL_FL, SPA_AUDIO_CHANNEL_FR }
	);
	params[param_count++] = spa_format_audio_raw_build(&builder, SPA_PARAM_EnumFormat, &audio_info);

	// alternative: F32LE, 44100Hz, stereo
	audio_info = SPA_AUDIO_INFO_RAW_INIT(
			.format = SPA_AUDIO_FORMAT_F32_LE,
			.flags = 0,
			.rate = 44100,
			.channels = 2,
			.position = { SPA_AUDIO_CHANNEL_FL, SPA_AUDIO_CHANNEL_FR }
	);
	params[param_count++] = spa_format_audio_raw_build(&builder, SPA_PARAM_EnumFormat, &audio_info);

	// alternative: F32LE, 48000Hz, stereo
	audio_info = SPA_AUDIO_INFO_RAW_INIT(
			.format = SPA_AUDIO_FORMAT_F32_LE,
			.flags = 0,
			.rate = 48000,
			.channels = 2,
			.position = { SPA_AUDIO_CHANNEL_FL, SPA_AUDIO_CHANNEL_FR }
	);
	params[param_count++] = spa_format_audio_raw_build(&builder, SPA_PARAM_EnumFormat, &audio_info);

	// alternative: S16LE, 44100Hz, mono (for compatibility)
	audio_info = SPA_AUDIO_INFO_RAW_INIT(
			.format = SPA_AUDIO_FORMAT_S16_LE,
			.flags = 0,
			.rate = 44100,
			.channels = 1,
			.position = { SPA_AUDIO_CHANNEL_MONO }
	);
	params[param_count++] = spa_format_audio_raw_build(&builder, SPA_PARAM_EnumFormat, &audio_info);

	// connect the stream
	pw_stream_flags flags = static_cast<pw_stream_flags>(
		PW_STREAM_FLAG_MAP_BUFFERS |
		PW_STREAM_FLAG_RT_PROCESS
	);

	int stream_connect = pw_stream_connect(
		capture_stream,
		PW_DIRECTION_INPUT,
		PW_ID_ANY,
		flags,
		params,
		param_count
	);

	if (stream_connect != 0) {
		fprintf(stderr, "Failed to connect stream: %d\n", stream_connect);
		return false;
	}

	if (thread_loop) {
		pw_thread_loop_unlock(thread_loop);
	}

	printf("Input node created & connected\n");
	return true;
}

bool SysAudioLinux::StartCapture(int exclude_pid, AudioDataCallback callback) {
	if (is_capturing) {
		return false;
	}

	excluded_pid = static_cast<pid_t>(exclude_pid);
	data_callback = callback;

	// filter target_nodes by excluded_pid
	for (auto it = target_nodes.begin(); it != target_nodes.end();) {
		if (sysaudio::utils::is_pid_descendant_of(it->second, excluded_pid)) {
			// remove node from target_nodes
			uint32_t node_id = it->first;
			it = target_nodes.erase(it);

			// remove any pending ports for this node
			pending_ports.erase(
				std::remove_if(pending_ports.begin(), pending_ports.end(), [node_id](const PendingPort &p) { return p.node_id == node_id; }),
				pending_ports.end()
			);
		} else {
			++it;
		}
	}

	// create input stream
	if (!create_input_node()) {
		fprintf(stderr, "Failed to create PipeWire input stream node\n");
		return false;
	}

	is_capturing = true;
	shared_buffer.set_capture_active(true);
	return true;
}

bool SysAudioLinux::stop_capture(bool preserve_state) {
	printf("stop_capture::start\n");

	if (thread_loop) {
		pw_thread_loop_lock(thread_loop);
	}

	is_capturing = false;
	excluded_pid = 0;
	shared_buffer.set_capture_active(false);

	// destroy active links
	for (auto &[node_id, links] : active_links) {
		for (auto &active_link : links) {
			pw_proxy_destroy(active_link.link);
		}
	}

	if (preserve_state) {
		// move active links info to pending_ports for reuse
		for (auto &[node_id, links] : active_links) {
			for (auto &active_link : links) {
				pending_ports.push_back({ active_link.node_id, active_link.port_id, active_link.channel });
			}
		}
	} else {
		// clear target_nodes when not preserving state
		target_nodes.clear();
	}

	// always clear active_links and stream_ports
	active_links.clear();
	stream_ports.clear();
	capture_stream_node_id = 0;

	{
		std::lock_guard<std::mutex> lock(callback_mutex);
		data_callback = nullptr;
	}

	if (capture_stream) {
		printf("Disconnecting input stream\n");
		pw_stream_disconnect(capture_stream);

		printf("Destroying input stream\n");
		pw_stream_destroy(capture_stream);

		capture_stream = nullptr;
	}

	if (thread_loop) {
		pw_thread_loop_unlock(thread_loop);
	}

	printf("stop_capture::ok\n");
	return true;
}

void SysAudioLinux::process_input_audio() {
	if (!is_capturing || !capture_stream) {
		return;
	}

	AudioDataCallback local_callback;
	{
		std::lock_guard<std::mutex> lock(callback_mutex);
		local_callback = data_callback;
	}

	const bool use_shared_buffer = !local_callback && shared_buffer.is_attached();

	if (!local_callback && !use_shared_buffer) {
		return;
	}

	pw_buffer *buf = pw_stream_dequeue_buffer(capture_stream);

	if (!buf) {
		return;
	}

	spa_buffer *spa_buf = buf->buffer;
	spa_data *data = &spa_buf->datas[0];

	if (data->data == nullptr || data->chunk == nullptr) {
		pw_stream_queue_buffer(capture_stream, buf);
		return;
	}

	size_t bytes = data->chunk->size;
	if (bytes == 0) {
		pw_stream_queue_buffer(capture_stream, buf);
		return;
	}

	if (use_shared_buffer) {
		shared_buffer.write_capture(static_cast<uint8_t *>(data->data), bytes);

		// return the PipeWire buffer immediately
		pw_stream_queue_buffer(capture_stream, buf);
		return;
	}

	AudioFormat *format = new AudioFormat;
	format->sampleRate = current_rate;
	format->channels = current_channels;
	format->bitsPerSample = sysaudio::utils::get_bytes_per_sample(static_cast<spa_audio_format>(current_format)) * 8;
	format->format = current_format;

	// copy data out of the PipeWire buffer before returning it
	uint8_t *copied = new uint8_t[bytes];
	memcpy(copied, data->data, bytes);

	AudioFrame *frame = new AudioFrame{
		copied,
		format,
		bytes
	};

	// return the PipeWire buffer immediately
	pw_stream_queue_buffer(capture_stream, buf);

	// now safe to call the callback with our own copy
	local_callback(frame);
}

void SysAudioLinux::input_param_changed(uint32_t id, const struct spa_pod *param) {
	if (param == nullptr || id != SPA_PARAM_Format) {
		return;
	}

	spa_audio_info_raw info = {};
	spa_zero(info);

	if (spa_format_audio_raw_parse(param, &info) < 0) {
		fprintf(stderr, "Failed to parse audio format\n");
		return;
	}

	current_format = static_cast<uint32_t>(info.format);
	current_rate = info.rate;
	current_channels = info.channels;

	shared_buffer.set_format(
		info.rate,
		info.channels,
		static_cast<uint32_t>(sysaudio::utils::get_bytes_per_sample(info.format) * 8),
		current_format
	);
}

void SysAudioLinux::input_state_changes(enum pw_stream_state /*old*/, enum pw_stream_state state, const char *error) {
	printf("Input stream state changed: %s\n", pw_stream_state_as_string(state));

	if (error) {
		printf("Stream error: %s\n", error);
	}

	if (state == PW_STREAM_STATE_STREAMING) {
		// update the stream node id
		capture_stream_node_id = pw_stream_get_node_id(capture_stream);
	} else if (state != PW_STREAM_STATE_STREAMING) {
		capture_stream_node_id = 0;
	}
}
