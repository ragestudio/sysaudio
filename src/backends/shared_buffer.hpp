#ifndef SYSAUDIO_SHARED_BUFFER_HPP
#define SYSAUDIO_SHARED_BUFFER_HPP

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace sysaudio {

// shared memory layout, keep in sync with the renderer layout constants
// header is 64 bytes, followed by the capture ring and the playback ring
constexpr uint32_t SHARED_MAGIC = 0x53594155;
constexpr uint32_t SHARED_VERSION = 1;
constexpr uint32_t SHARED_HEADER_BYTES = 64;

// header word offsets (bytes)
constexpr uint32_t SHARED_MAGIC_OFFSET = 0;
constexpr uint32_t SHARED_VERSION_OFFSET = 4;
constexpr uint32_t SHARED_SAMPLE_RATE_OFFSET = 8;
constexpr uint32_t SHARED_CHANNELS_OFFSET = 12;
constexpr uint32_t SHARED_BITS_PER_SAMPLE_OFFSET = 16;
constexpr uint32_t SHARED_FORMAT_OFFSET = 20;
constexpr uint32_t SHARED_CAPTURE_WRITE_OFFSET = 24;
constexpr uint32_t SHARED_CAPTURE_READ_OFFSET = 28;
constexpr uint32_t SHARED_PLAYBACK_WRITE_OFFSET = 32;
constexpr uint32_t SHARED_PLAYBACK_READ_OFFSET = 36;
constexpr uint32_t SHARED_CAPTURE_CAPACITY_OFFSET = 40;
constexpr uint32_t SHARED_PLAYBACK_CAPACITY_OFFSET = 44;
constexpr uint32_t SHARED_CAPTURE_ACTIVE_OFFSET = 48;

// single producer / single consumer ring buffer placed in shared memory
class SharedAudioBuffer {
   public:
	void attach(uint8_t *base, size_t length) {
		detach();

		if (!base || length < SHARED_HEADER_BYTES) {
			return;
		}

		if (reinterpret_cast<uintptr_t>(base) % alignof(std::atomic<uint32_t>) != 0) {
			return;
		}

		base_ = base;
		length_ = length;

		if (word(SHARED_MAGIC_OFFSET)->load(std::memory_order_relaxed) != SHARED_MAGIC) {
			detach();
			return;
		}

		const uint32_t capture_capacity = word(SHARED_CAPTURE_CAPACITY_OFFSET)->load(std::memory_order_relaxed);
		const uint32_t playback_capacity = word(SHARED_PLAYBACK_CAPACITY_OFFSET)->load(std::memory_order_relaxed);

		if (!is_power_of_two(capture_capacity) || !is_power_of_two(playback_capacity)) {
			detach();
			return;
		}

		const size_t required = static_cast<size_t>(SHARED_HEADER_BYTES) + capture_capacity + playback_capacity;

		if (required > length) {
			detach();
			return;
		}

		capture_capacity_ = capture_capacity;
		playback_capacity_ = playback_capacity;
		capture_data_ = base_ + SHARED_HEADER_BYTES;
		playback_data_ = capture_data_ + capture_capacity_;

		word(SHARED_CAPTURE_ACTIVE_OFFSET)->store(0, std::memory_order_release);
	}

	void detach() {
		base_ = nullptr;
		length_ = 0;
		capture_capacity_ = 0;
		playback_capacity_ = 0;
		capture_data_ = nullptr;
		playback_data_ = nullptr;
	}

	bool is_attached() const {
		return base_ != nullptr;
	}

	// called from the capture thread, drops the frame when there is no space
	size_t write_capture(const uint8_t *data, size_t size) {
		if (!base_ || !data || size == 0) {
			return 0;
		}

		const uint32_t capacity = capture_capacity_;
		const uint32_t mask = capacity - 1;

		const uint32_t write = word(SHARED_CAPTURE_WRITE_OFFSET)->load(std::memory_order_relaxed);
		const uint32_t read = word(SHARED_CAPTURE_READ_OFFSET)->load(std::memory_order_acquire);
		const uint32_t available = capacity - (write - read);

		if (size > available) {
			return 0;
		}

		const size_t offset = write & mask;
		const size_t first = std::min<size_t>(size, capacity - offset);

		memcpy(capture_data_ + offset, data, first);

		if (first < size) {
			memcpy(capture_data_, data + first, size - first);
		}

		word(SHARED_CAPTURE_WRITE_OFFSET)->store(write + static_cast<uint32_t>(size), std::memory_order_release);

		return size;
	}

	// called from the output thread
	size_t read_playback(uint8_t *out, size_t size) {
		if (!base_ || !out || size == 0) {
			return 0;
		}

		const uint32_t capacity = playback_capacity_;
		const uint32_t mask = capacity - 1;

		const uint32_t read = word(SHARED_PLAYBACK_READ_OFFSET)->load(std::memory_order_relaxed);
		const uint32_t write = word(SHARED_PLAYBACK_WRITE_OFFSET)->load(std::memory_order_acquire);

		const uint32_t available = write - read;

		if (size > available) {
			size = available;
		}

		const size_t offset = read & mask;
		const size_t first = std::min<size_t>(size, capacity - offset);

		memcpy(out, playback_data_ + offset, first);

		if (first < size) {
			memcpy(out + first, playback_data_, size - first);
		}

		word(SHARED_PLAYBACK_READ_OFFSET)->store(read + static_cast<uint32_t>(size), std::memory_order_release);

		return size;
	}

	void set_format(uint32_t sample_rate, uint32_t channels, uint32_t bits_per_sample, uint32_t format) {
		if (!base_) {
			return;
		}

		if (sample_rate == 0 || channels == 0 || bits_per_sample == 0) {
			return;
		}

		word(SHARED_SAMPLE_RATE_OFFSET)->store(sample_rate, std::memory_order_relaxed);
		word(SHARED_CHANNELS_OFFSET)->store(channels, std::memory_order_relaxed);
		word(SHARED_BITS_PER_SAMPLE_OFFSET)->store(bits_per_sample, std::memory_order_relaxed);
		word(SHARED_FORMAT_OFFSET)->store(format, std::memory_order_relaxed);
	}

	void set_capture_active(bool active) {
		if (!base_) {
			return;
		}

		word(SHARED_CAPTURE_ACTIVE_OFFSET)->store(active ? 1 : 0, std::memory_order_release);
	}

   private:
	std::atomic<uint32_t> *word(uint32_t offset) const {
		return reinterpret_cast<std::atomic<uint32_t> *>(base_ + offset);
	}

	static bool is_power_of_two(uint32_t value) {
		return value > 0 && (value & (value - 1)) == 0;
	}

	uint8_t *base_ = nullptr;
	size_t length_ = 0;

	uint32_t capture_capacity_ = 0;
	uint32_t playback_capacity_ = 0;

	uint8_t *capture_data_ = nullptr;
	uint8_t *playback_data_ = nullptr;
};

}  // namespace sysaudio

#endif	// SYSAUDIO_SHARED_BUFFER_HPP
