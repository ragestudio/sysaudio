#ifndef SYSAUDIO_AUDIO_BACKEND_HPP
#define SYSAUDIO_AUDIO_BACKEND_HPP

#include <cstddef>
#include <cstdint>

#include "../types.hpp"
#include "ringbuffer.hpp"
#include "shared_buffer.hpp"

// common backend interface. any platform implementation can derive from this
// and get the shared memory transport for free
class AudioBackend {
   public:
	virtual ~AudioBackend() = default;

	virtual bool Initialize(const InitializeParams &params) = 0;
	virtual void Stop() = 0;

	virtual bool StartCapture(int exclude_pid, AudioDataCallback callback) = 0;
	virtual void StopCapture() = 0;

	virtual void Output(AudioFrame *frame) = 0;
	virtual AudioFormat GetFormat() const = 0;

	void AttachSharedBuffer(uint8_t *data, size_t length) {
		shared_buffer.attach(data, length);
	}

	void DetachSharedBuffer() {
		shared_buffer.detach();
	}

   protected:
	sysaudio::SharedAudioBuffer shared_buffer;
	sysaudio::ring_buffer::RingBuffer output_buffer{ 8 * 1024 };
};

#endif	// SYSAUDIO_AUDIO_BACKEND_HPP
