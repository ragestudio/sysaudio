#include <cstdint>
#include <cstdio>

#include "engine.hpp"

SysAudioLinux::~SysAudioLinux() = default;
SysAudioLinux::SysAudioLinux() : impl(std::make_unique<pipewire::PipewireAudio>()) {
}

bool SysAudioLinux::Initialize(InitializeParams params) {
	printf("[SysAudio] Initialized with params:\n");
	printf("  excluded_pid: %d\n", params.excluded_pid);
	printf("  node_name: %s\n", params.node_name.c_str());
	printf("  device_app_name: %s\n", params.device_app_name.c_str());
	printf("  device_app_id: %s\n", params.device_app_id.c_str());
	printf("  device_app_icon_name: %s\n", params.device_app_icon_name.c_str());

	bool ok = impl->initialize(params);

	if (!ok) {
		fprintf(stderr, "Failed to initialize sysaudio engine\n");
		return false;
	}

	return true;
}

bool SysAudioLinux::StartCapture(pid_t excludePid, AudioDataCallback dataCallback) {
	return impl->start_capture(excludePid, dataCallback);
}

void SysAudioLinux::StopCapture() {
	impl->stop_capture();
}

void SysAudioLinux::Stop() {
	impl->stop();
}

void SysAudioLinux::Output(AudioFrame *frame) {
	if (!frame) {
		return;
	}

	impl->push_to_output_buffer(frame);
}

void SysAudioLinux::SetAudioFormat(spa_audio_format format, uint32_t rate, uint32_t channels) {
	impl->set_audio_format(format, rate, channels);
}

spa_audio_format SysAudioLinux::GetFormat() const {
	return static_cast<spa_audio_format>(impl->get_format());
}

uint32_t SysAudioLinux::GetSampleRate() const {
	return impl->get_sample_rate();
}

uint32_t SysAudioLinux::GetChannels() const {
	return impl->get_channels();
}

size_t SysAudioLinux::GetBytesPerSample() const {
	return impl->get_bytes_per_sample();
}

AudioFormat SysAudioLinux::GetCurrentFormat() const {
	AudioFormat format;
	format.sampleRate = GetSampleRate();
	format.channels = GetChannels();
	format.bitsPerSample = GetBytesPerSample() * 8;
	format.format = GetFormat();
	return format;
}
