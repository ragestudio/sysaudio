#ifndef AUDIOCALLBACK_HPP
#define AUDIOCALLBACK_HPP

#include <sched.h>

#include <cstdint>
#include <functional>
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

struct InitializeParams {
#ifdef __linux
	pid_t excluded_pid;
	std::string node_name;
	std::string device_app_name;
	std::string device_app_id;
	std::string device_app_icon_name;
#endif
#ifdef _WIN32
	DWORD excluded_pid;
#endif
};

struct AudioFormat {
	uint32_t sampleRate;
	uint32_t channels;
	uint32_t bitsPerSample;
	uint32_t format;  // spa_audio_format value
};

struct AudioFrame {
	uint8_t *buff;
	AudioFormat *format;
	size_t size;
};

using AudioDataCallback = std::function<void(AudioFrame *frame)>;

#endif	// AUDIOCALLBACK_HPP
