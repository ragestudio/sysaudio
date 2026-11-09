// shared memory layout helpers for the sysaudio tests and examples.
// keep in sync with src/shared_buffer.hpp and the renderer implementations
export const HEADER_BYTES = 64
export const HEADER_WORDS = HEADER_BYTES / 4

export const CAPTURE_CAPACITY = 256 * 1024
export const PLAYBACK_CAPACITY = 256 * 1024

export const SHARED_MAGIC = 0x53594155
export const SHARED_VERSION = 1

export const SharedWord = {
	Magic: 0,
	Version: 1,
	SampleRate: 2,
	Channels: 3,
	BitsPerSample: 4,
	Format: 5,
	CaptureWrite: 6,
	CaptureRead: 7,
	PlaybackWrite: 8,
	PlaybackRead: 9,
	CaptureCapacity: 10,
	PlaybackCapacity: 11,
	CaptureActive: 12,
}

export function createSharedAudioBuffer() {
	const buffer = new SharedArrayBuffer(
		HEADER_BYTES + CAPTURE_CAPACITY + PLAYBACK_CAPACITY,
	)
	const header = new Int32Array(buffer, 0, HEADER_WORDS)

	header[SharedWord.Magic] = SHARED_MAGIC
	header[SharedWord.Version] = SHARED_VERSION
	header[SharedWord.CaptureCapacity] = CAPTURE_CAPACITY
	header[SharedWord.PlaybackCapacity] = PLAYBACK_CAPACITY

	return {
		buffer,
		header,
		capture: new Uint8Array(buffer, HEADER_BYTES, CAPTURE_CAPACITY),
		captureS16: new Int16Array(buffer, HEADER_BYTES, CAPTURE_CAPACITY / 2),
		playback: new Uint8Array(
			buffer,
			HEADER_BYTES + CAPTURE_CAPACITY,
			PLAYBACK_CAPACITY,
		),
		playbackS16: new Int16Array(
			buffer,
			HEADER_BYTES + CAPTURE_CAPACITY,
			PLAYBACK_CAPACITY / 2,
		),
	}
}

export function readFormat(header) {
	return {
		sampleRate: Atomics.load(header, SharedWord.SampleRate),
		channels: Atomics.load(header, SharedWord.Channels),
		bitsPerSample: Atomics.load(header, SharedWord.BitsPerSample),
		format: Atomics.load(header, SharedWord.Format),
		captureActive: Atomics.load(header, SharedWord.CaptureActive),
	}
}

export function captureStats(header) {
	const write = Atomics.load(header, SharedWord.CaptureWrite)
	const read = Atomics.load(header, SharedWord.CaptureRead)

	return {
		write,
		read,
		available: (write - read) >>> 0,
	}
}

export function playbackStats(header) {
	const write = Atomics.load(header, SharedWord.PlaybackWrite)
	const read = Atomics.load(header, SharedWord.PlaybackRead)

	return {
		write,
		read,
		available: (write - read) >>> 0,
	}
}

// copies up to target.length bytes out of the capture ring and advances read
export function readCapture(shared, target) {
	const capacity = shared.capture.length
	const mask = capacity - 1

	let read = Atomics.load(shared.header, SharedWord.CaptureRead)
	const write = Atomics.load(shared.header, SharedWord.CaptureWrite)
	const available = (write - read) >>> 0
	const size = Math.min(target.length, available)

	if (size === 0) {
		return 0
	}

	const offset = read & mask
	const first = Math.min(size, capacity - offset)

	target.set(shared.capture.subarray(offset, offset + first), 0)

	if (first < size) {
		target.set(shared.capture.subarray(0, size - first), first)
	}

	read = (read + size) >>> 0

	Atomics.store(shared.header, SharedWord.CaptureRead, read)

	return size
}

// writes bytes into the playback ring, mirroring what the audio worklet does.
// returns the number of bytes written, drops the rest when the ring is full
export function writePlayback(shared, data) {
	const capacity = shared.playback.length
	const mask = capacity - 1

	let write = Atomics.load(shared.header, SharedWord.PlaybackWrite)
	const read = Atomics.load(shared.header, SharedWord.PlaybackRead)
	const available = capacity - ((write - read) >>> 0)
	const size = Math.min(data.length, available)

	if (size === 0) {
		return 0
	}

	const offset = write & mask
	const first = Math.min(size, capacity - offset)

	shared.playback.set(data.subarray(0, first), offset)

	if (first < size) {
		shared.playback.set(data.subarray(first, size), 0)
	}

	write = (write + size) >>> 0

	Atomics.store(shared.header, SharedWord.PlaybackWrite, write)

	return size
}

export function createTone({
	frames = 1024,
	sampleRate = 44100,
	channels = 2,
	frequency = 440,
	amplitude = 0.25,
} = {}) {
	const bytesPerSample = 2
	const buffer = Buffer.alloc(frames * channels * bytesPerSample)

	let phase = 0

	for (let i = 0; i < frames; i++) {
		const sample = Math.round(Math.sin(phase) * amplitude * 0x7fff)

		phase += (2 * Math.PI * frequency) / sampleRate

		for (let channel = 0; channel < channels; channel++) {
			buffer.writeInt16LE(sample, (i * channels + channel) * 2)
		}
	}

	return buffer
}

export function measurePeak(buffer) {
	let peak = 0

	for (let i = 0; i + 1 < buffer.length; i += 2) {
		const value = Math.abs(buffer.readInt16LE(i))

		if (value > peak) {
			peak = value
		}
	}

	return peak
}
